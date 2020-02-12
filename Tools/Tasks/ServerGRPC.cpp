#include "pch.h"
#include "ServerGRPC.h"

#include "sensorStreaming.grpc.pb.h"

// From gRPC proto file
using sensorStreaming::Streamer;
using sensorStreaming::NameRPC;
using sensorStreaming::CameraIntrinsicsRPC;
using sensorStreaming::PoseRPC;
using sensorStreaming::ImageRPC;
using sensorStreaming::SensorFrameRPC;

class StreamerServiceImpl final : public Streamer::Service
{
	grpc::Status GetCamIntrinsics(
		grpc::ServerContext* context,
		const NameRPC* camera_name,
		CameraIntrinsicsRPC* camIntrinsics) override
	{
		camIntrinsics->set_fy(1);
		camIntrinsics->set_fy(2);
		camIntrinsics->set_cx(3);
		camIntrinsics->set_cy(4);
		return grpc::Status::OK;
	}

	grpc::Status SensorStream(
		grpc::ServerContext* context,
		const NameRPC* camera_name,
		grpc::ServerWriter<SensorFrameRPC>* writer) override
	{
		return grpc::Status::OK;
	}
};

namespace Tasks {

std::vector<HoloLensForCV::SensorFrame^> sensorFrames;

Tasks::ServerGRPC::ServerGRPC()
{
	m_mediaFrameSourceGroupType = HoloLensForCV::MediaFrameSourceGroupType::HoloLensResearchModeSensors;

	// Sensors to be enabled
	m_enabledSensorTypes.emplace_back(
		HoloLensForCV::SensorType::VisibleLightLeftFront);

	m_enabledSensorTypes.emplace_back(
		HoloLensForCV::SensorType::VisibleLightRightFront);

	// TODO move to "credentials" file (+encryption ?)
	m_serverAddress = "10.10.255.241:50051";

	m_mediaFrameSourceGroupStarted = false;
	m_isRunning = false;
	m_processedFrames = 0;
}

void Tasks::ServerGRPC::Run(IBackgroundTaskInstance ^ taskInstance)
{
	if (!m_isRunning)
	{
		StartHoloLensMediaFrameSourceGroup();
		Serve();
	}
	m_server->Wait();

	// Main background loop to update sensors data
	while (m_isRunning)
	{
		Concurrency::create_task(StreamAsync()).then(
			[&]()
			{
				m_processedFrames++;
				if (m_processedFrames % 10 == 0)
					dbg::trace(L"Number of processed frames so far: %i", m_processedFrames);
			});
	}
}

void Tasks::ServerGRPC::Serve()
{
	StreamerServiceImpl m_streamerService;
	// Listener
	m_serverBuilder.AddListeningPort(m_serverAddress, grpc::InsecureServerCredentials());
	// Register service instance
	m_serverBuilder.RegisterService(&m_streamerService);
	// Assemble server
	m_server = std::unique_ptr<grpc::Server>(m_serverBuilder.BuildAndStart());
	// Server is up
	dbg::trace(L"Server listening on %s", m_serverAddress);
	m_isRunning = true;
}

void Tasks::ServerGRPC::Stop()
{
	if (m_isRunning)
	{
		m_server->Shutdown();
		m_isRunning = false;
	}
}

Concurrency::task<void> ServerGRPC::StreamAsync()
{
	dbg::TimerGuard timerGuard(
		L"ServerGRPC::StreamAsync",
		30.0 /* minimum_time_elapsed_in_milliseconds */);

	Concurrency::task<void> streamTask = Concurrency::task_from_result();

	if (!m_mediaFrameSourceGroupStarted)
		return streamTask;

	sensorFrames.clear();
	int count = 0;
	// Read sensor frames
	for (const auto sensorType : m_enabledSensorTypes)
	{
		auto frame = m_multiFrameBuffer->GetLatestFrame(sensorType);
		if (frame == nullptr)
		{
			dbg::trace(L"Frame is null for sensor %s", sensorType.ToString());
			continue; // Skip this frame's processing, sensor not available
		}
		count++;
		sensorFrames.push_back(frame);
	}
	dbg::trace(L"%i frames updated", count);
	return streamTask;
}

void Tasks::ServerGRPC::StartHoloLensMediaFrameSourceGroup()
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
		});
}

//HoloLensForCV::SensorType ServerGRPC::FromStringNameToSensorType(std::string sensorName)
//{
//	if (sensorName == "pv")
//	{
//		return HoloLensForCV::SensorType::PhotoVideo;
//	}
//	if (sensorName == "vlc_ll")
//	{
//		return HoloLensForCV::SensorType::VisibleLightLeftLeft;
//	}
//	if (sensorName == "vlc_lf")
//	{
//		return HoloLensForCV::SensorType::VisibleLightLeftFront;
//	}
//	if (sensorName == "vlc_rf")
//	{
//		return HoloLensForCV::SensorType::VisibleLightRightFront;
//	}
//	if (sensorName == "vlc_rr")
//	{
//		return HoloLensForCV::SensorType::VisibleLightRightRight;
//	}
//	dbg::trace(L"Invalid of unsupported sensor.");
//	return HoloLensForCV::SensorType::Undefined;
//}
}
