#include "pch.h"
#include "StreamerServiceImpl.h"


ImageRPC MakeImageRPC(Windows::Graphics::Imaging::SoftwareBitmap^ bitmap)
{
	ImageRPC imageRPC;
	imageRPC.set_height(bitmap->PixelHeight);
	imageRPC.set_width(bitmap->PixelWidth);
	//Windows::Storage::Streams::Buffer^ buffer;
	//bitmap->CopyToBuffer(buffer);
	//imageRPC.set_data(reinterpret_cast<char*>(buffer));
	// TODO fix bitmap->bytes conversion

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
	dbg::trace(L"Invalid of unsupported sensor.");
	return HoloLensForCV::SensorType::Undefined;
}

StreamerServiceImpl::~StreamerServiceImpl()
{
	dbg::trace(L"Shutting down server");
	m_server->Shutdown();
	m_cq->Shutdown();
}

void StreamerServiceImpl::Run()
{
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
	dbg::trace(L"Server listening", server_address.c_str());

	// Instantiate first CallData object to handle incoming requests
	new CallData(&m_service, m_cq.get(), INTRINSICS);
	new CallData(&m_service, m_cq.get(), SENSORSTREAM);
}

Concurrency::task<void> StreamerServiceImpl::HandleRpcs(std::vector<HoloLensForCV::SensorFrame^> sensorFrames)
{
	Concurrency::task<void> handleTask =
		Concurrency::task_from_result();

	void* tag;
	bool ok;
	// Check completion queue status for any pending request
	GPR_ASSERT(m_cq->Next(&tag, &ok));
	GPR_ASSERT(ok);
	CallData* cd = static_cast<CallData*>(tag);
	cd->UpdateSensorData(sensorFrames);
	cd->Proceed();

	return handleTask;
}

void CallData::Proceed()
{
	CameraIntrinsicsRPC camIntrinsics;
	switch (m_status)
	{
	case CREATE:
		m_status = PROCESS;

		if (m_type == INTRINSICS)
			m_service->RequestGetCamIntrinsics(&m_context, &m_sensorRequest,
				&m_responderIntrinsics, m_cq, m_cq, this);
		else if (m_type == SENSORSTREAM)
			m_service->RequestSensorStream(&m_context, &m_sensorRequest,
				&m_writerSensorStreaming, m_cq, m_cq, this);
		break;
	case PROCESS:
		new CallData(m_service, m_cq, m_type);

		if (m_type == INTRINSICS)
		{
			// Process request
			dbg::trace(L"Processing getIntrinsics request");
			camIntrinsics.set_fx(1);
			camIntrinsics.set_fy(2);
			camIntrinsics.set_cx(3);
			camIntrinsics.set_cy(4);
			// Response ready
			m_responderIntrinsics.Finish(camIntrinsics, grpc::Status::OK, this);
			m_status = FINISH;
		}
		else if (m_type == SENSORSTREAM)
		{
			if (m_sensorFrames.empty() || m_count == 10)
			{
				m_status = FINISH;
				m_writerSensorStreaming.WriteAndFinish(SensorFrameRPC(),
					m_writeOptions, grpc::Status::OK, this);
				break;
			}
			HoloLensForCV::SensorType camera_name = ParseNameRPC(m_sensorRequest);
			dbg::trace(L"Processing sensorStream request");
			for (const auto sensorFrame : m_sensorFrames)
			{
				if (sensorFrame->FrameType == camera_name)
				{
					m_writerSensorStreaming.Write(MakeSensorFrameRPC(sensorFrame), this);
					m_count++;
					break;
				}
			}
		}
		break;
	case FINISH:
		dbg::trace(L"Deallocate CallData object");
		delete this;
		break;
	default:
		break;
	}
}