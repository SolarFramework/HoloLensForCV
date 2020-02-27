#include "pch.h"
#include "ServerGRPC.h"

namespace Tasks {

ImageRPC MakeImageRPC(Windows::Graphics::Imaging::SoftwareBitmap^ bitmap)
{
	// Bitmap to buffer data
	Windows::Graphics::Imaging::BitmapBuffer^ bitmapBuffer =
		bitmap->LockBuffer(Windows::Graphics::Imaging::BitmapBufferAccessMode::Read);
	Windows::Foundation::IMemoryBufferReference^ bitmapBufferRef =
		bitmapBuffer->CreateReference();
	uint32_t bitmapBufferDataSize = 0;
	uint8_t* bitmapBufferData =
		Io::GetTypedPointerToMemoryBuffer<uint8_t>(
			bitmapBufferRef,
			bitmapBufferDataSize);
	// Pixel format handling
	int32_t pixelStride;
	switch (bitmap->BitmapPixelFormat)
	{
	case Windows::Graphics::Imaging::BitmapPixelFormat::Bgra8:
		pixelStride = 4;
		break;
	case Windows::Graphics::Imaging::BitmapPixelFormat::Gray16:
		pixelStride = 2;
		break;
	case Windows::Graphics::Imaging::BitmapPixelFormat::Gray8:
		pixelStride = 1;
		break;
	default:
		dbg::trace(L"Unrecognized pixel format, assuming 1 byte per pixel.");
		pixelStride = 1;
		break;
	}
	int32_t imageBufferSize = bitmap->PixelHeight * bitmap->PixelWidth * pixelStride;
	ASSERT(imageBufferSize == (int32_t)bitmapBufferDataSize);

	ImageRPC imageRPC;
	imageRPC.set_height(bitmap->PixelHeight);
	imageRPC.set_width(bitmap->PixelWidth * pixelStride);

	imageRPC.mutable_data()->append(reinterpret_cast<char*>(bitmapBufferData));

	return imageRPC;
}

MatRPC MakeMatRPC(Windows::Foundation::Numerics::float4x4 mat)
{
	MatRPC matRPC;
	matRPC.set_m11(mat.m11);
	matRPC.set_m12(mat.m12);
	matRPC.set_m13(mat.m13);
	matRPC.set_m14(mat.m14);
	matRPC.set_m21(mat.m21);
	matRPC.set_m22(mat.m22);
	matRPC.set_m23(mat.m23);
	matRPC.set_m24(mat.m24);
	matRPC.set_m31(mat.m31);
	matRPC.set_m32(mat.m32);
	matRPC.set_m33(mat.m33);
	matRPC.set_m34(mat.m34);
	matRPC.set_m41(mat.m41);
	matRPC.set_m42(mat.m42);
	matRPC.set_m43(mat.m43);
	matRPC.set_m44(mat.m44);

	return matRPC;
}

PoseRPC MakePoseRPC(HoloLensForCV::SensorFrame^ sensorFrame)
{
	PoseRPC pose;
	pose.mutable_cameraproj()->CopyFrom(MakeMatRPC(sensorFrame->CameraProjectionTransform));
	pose.mutable_cameraview()->CopyFrom(MakeMatRPC(sensorFrame->CameraViewTransform));
	pose.mutable_frametoorigin()->CopyFrom(MakeMatRPC(sensorFrame->FrameToOrigin));

	return pose;
}

SensorFrameRPC MakeSensorFrameRPC(HoloLensForCV::SensorFrame^ sensorFrame)
{
	SensorFrameRPC sensorFrameRPC;
	sensorFrameRPC.mutable_image()->CopyFrom(MakeImageRPC(sensorFrame->SoftwareBitmap));
	sensorFrameRPC.mutable_pose()->CopyFrom(MakePoseRPC(sensorFrame));
	sensorFrameRPC.set_timestamp(sensorFrame->Timestamp.UniversalTime);

	return sensorFrameRPC;
}

HoloLensForCV::SensorType ParseNameRPC(NameRPC name)
{
	if (name.cameraname() == "pv")
	{
		return HoloLensForCV::SensorType::PhotoVideo;
	}
	if (name.cameraname() == "vlc_ll")
	{
		return HoloLensForCV::SensorType::VisibleLightLeftLeft;
	}
	if (name.cameraname() == "vlc_lf")
	{
		return HoloLensForCV::SensorType::VisibleLightLeftFront;
	}
	if (name.cameraname() == "vlc_rf")
	{
		return HoloLensForCV::SensorType::VisibleLightRightFront;
	}
	if (name.cameraname() == "vlc_rr")
	{
		return HoloLensForCV::SensorType::VisibleLightRightRight;
	}
	dbg::trace(L"Invalid or unsupported sensor.");
	return HoloLensForCV::SensorType::Undefined;
}


ServerGRPC::ServerGRPC() :
	m_mediaFrameSourceGroupType(HoloLensForCV::MediaFrameSourceGroupType::HoloLensResearchModeSensors),
	m_mediaFrameSourceGroupStarted(false),
	m_isRunning(false)
{
	// Sensors to be enabled
	//m_enabledSensorTypes.emplace_back(
	//	HoloLensForCV::SensorType::VisibleLightLeftFront);

	//m_enabledSensorTypes.emplace_back(
	//	HoloLensForCV::SensorType::VisibleLightRightFront);

	//m_enabledSensorTypes.emplace_back(
	//	HoloLensForCV::SensorType::VisibleLightLeftLeft);

	//m_enabledSensorTypes.emplace_back(
	//	HoloLensForCV::SensorType::VisibleLightRightRight);
}

void ServerGRPC::Run(IBackgroundTaskInstance ^ taskInstance)
{
	if (!m_isRunning)
	{
		Serve();
	}
	if (!m_mediaFrameSourceGroupStarted)
	{
		//StartHoloLensMediaFrameSourceGroup();
	}

	dbg::trace(L"Beginning main loop");

	// Main loop to update sensors data & handle requests
	while (m_isRunning)
	{
		HandleRpcs();
	}
}

void ServerGRPC::Serve()
{
	m_isRunning = true;

	std::string server_address("0.0.0.0:50051");

	ServerBuilder builder;
	// Listener
	builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
	// Register service instance
	builder.RegisterService(&m_service);
	// Add completion queue to handle request asynchronously
	m_cq = builder.AddCompletionQueue();
	// Assemble server
	m_server = builder.BuildAndStart();

	// Instantiate first CallData object to handle incoming requests
	Proceed(new CallData(&m_service, m_cq.get(), ENABLE));
	Proceed(new CallData(&m_service, m_cq.get(), INTRINSICS));
}

void ServerGRPC::HandleRpcs()
{
	void* tag;
	bool ok;
	// Check completion queue status for any pending request
	dbg::trace(L"Waiting for next event in completion queue...");
	GPR_ASSERT(m_cq->Next(&tag, &ok));
	GPR_ASSERT(ok);
	CallData* cd = static_cast<CallData*>(tag);
	// Update sensors data for the gRPC app
	StreamAsync();
	// Process the event on top of the queue
	Proceed(cd);
}

void ServerGRPC::Proceed(CallData * cd)
{
	auto status = cd->getStatus();
	auto type = cd->getType();
	CameraIntrinsicsRPC camIntrinsics;
	switch (status)
	{
	case CREATE:
		cd->RegisterRequest(); // Handled internally
		break;
	case PROCESS:
		// Create new instance
		Proceed(new CallData(cd->m_service, cd->m_cq, type));
		if (type == ENABLE)
		{
			dbg::trace(L"Processing enableSensors request");
			cd->setStatus(FINISH);
			if (m_mediaFrameSourceGroupStarted)
			{
				dbg::trace(L"Media frame group already started, aborting");
				cd->m_responderEnable.Finish(SensorListRPC(), grpc::Status::CANCELLED, cd);
			}
			else
			{
				m_multiFrameBuffer = ref new HoloLensForCV::MultiFrameBuffer();

				m_spatialPerception = ref new HoloLensForCV::SpatialPerception();

				m_mediaFrameSourceGroup =
					ref new HoloLensForCV::MediaFrameSourceGroup(
						m_mediaFrameSourceGroupType,
						m_spatialPerception,
						m_multiFrameBuffer);

				for (NameRPC sensorName : cd->m_enableRequest.sensor())
				{
					auto sensorType = ParseNameRPC(sensorName);
					m_mediaFrameSourceGroup->Enable(sensorType);
					m_enabledSensorTypes.emplace_back(sensorType);
					dbg::trace(L"Enable %s", sensorType.ToString()->Data());
				}
				//StartHoloLensMediaFrameSourceGroup();

				concurrency::create_task(m_mediaFrameSourceGroup->StartAsync()).then(
					[&]()
					{
						dbg::trace(L"Media frame source group started.");
						m_mediaFrameSourceGroupStarted = true;
						// Tell the client the sensors are ready
						cd->m_responderEnable.Finish(cd->m_enableRequest, grpc::Status::OK, cd);
						// Begin listening to SensorStream requests
						Proceed(new CallData(&m_service, m_cq.get(), SENSORSTREAM));
					}).wait();
			}
		}
		else if (type == INTRINSICS)
		{
			// Process request
			dbg::trace(L"Processing dummy getIntrinsics request");
			camIntrinsics.set_fx(1); // Dummy response, for debug purpose
			camIntrinsics.set_fy(2);
			camIntrinsics.set_cx(3);
			camIntrinsics.set_cy(4);
			// Response ready
			cd->setStatus(FINISH);
			cd->m_responderIntrinsics.Finish(camIntrinsics, grpc::Status::OK, cd);
		}
		else if (type == SENSORSTREAM)
		{
			if (!m_mediaFrameSourceGroupStarted)
			{
				cd->setStatus(FINISH);
				cd->m_writerSensorStreaming.Finish(grpc::Status::CANCELLED, cd);
			}
			else if (m_sensorFrames.empty())
			{
				cd->setStatus(FINISH);
				cd->m_writerSensorStreaming.WriteAndFinish(SensorFrameRPC(),
					cd->m_writeOptions, grpc::Status::OK, cd);
			}
			else
			{
				HoloLensForCV::SensorType camera_name = ParseNameRPC(cd->m_sensorRequest);
				dbg::trace(L"Processing sensorStream request");
				for (const auto sensorFrame : m_sensorFrames)
				{
					if (sensorFrame->FrameType == camera_name)
					{
						cd->m_writerSensorStreaming.Write(MakeSensorFrameRPC(sensorFrame), cd);
						break;
					}
				}
			}
		}
		break;
	case FINISH:
		dbg::trace(L"Deallocate CallData object");
		delete cd;
		break;
	default:
		break;
	}
}

void ServerGRPC::Stop()
{
	if (m_isRunning)
	{
		m_isRunning = false;
		dbg::trace(L"Shutting down server");
		m_server->Shutdown();
		m_cq->Shutdown();
	}

	concurrency::create_task(m_mediaFrameSourceGroup->StopAsync()).then(
		[&]()
		{
			delete m_mediaFrameSourceGroup;
			m_mediaFrameSourceGroup = nullptr;
			m_mediaFrameSourceGroupStarted = false;

			delete m_spatialPerception;
			m_spatialPerception = nullptr;
			delete m_multiFrameBuffer;
			m_multiFrameBuffer = nullptr;
			dbg::trace(L"Stopping mediaFrameSourceGroup");
		}).wait();
}

void ServerGRPC::StreamAsync()
{	
	if (!m_mediaFrameSourceGroupStarted)
		return;

	m_sensorFrames.clear();
	m_count = 0;
	// Read sensor frames
	for (const auto sensorType : m_enabledSensorTypes)
	{
		m_currentFrame = m_multiFrameBuffer->GetLatestFrame(sensorType);
		if (m_currentFrame == nullptr)
		{
			// Skip this frame's processing, sensor not available
			dbg::trace(L"Frame is null for sensor %s", sensorType.ToString()->Data());
		}
		else
		{
			m_count++;
			m_sensorFrames.push_back(m_currentFrame);
		}
	}
	dbg::trace(L"Updated %i sensors", m_count);
	return;
}

void ServerGRPC::StartHoloLensMediaFrameSourceGroup()
{
	m_multiFrameBuffer = ref new HoloLensForCV::MultiFrameBuffer();

	m_spatialPerception = ref new HoloLensForCV::SpatialPerception();

	m_mediaFrameSourceGroup =
		ref new HoloLensForCV::MediaFrameSourceGroup(
			m_mediaFrameSourceGroupType,
			m_spatialPerception,
			m_multiFrameBuffer);

	for (const auto enabledSensorType : m_enabledSensorTypes)
	{
		m_mediaFrameSourceGroup->Enable(
			enabledSensorType);
	}
	concurrency::create_task(m_mediaFrameSourceGroup->StartAsync()).then(
		[&]()
		{
			dbg::trace(L"Media frame source group started.");
			m_mediaFrameSourceGroupStarted = true;
			// Begin listening to SensorStream requests
			Proceed(new CallData(&m_service, m_cq.get(), SENSORSTREAM));
		});
}

}


