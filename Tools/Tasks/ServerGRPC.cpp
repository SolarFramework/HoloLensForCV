#include "pch.h"
#include "ServerGRPC.h"

namespace Tasks {

ServerGRPC::ServerGRPC()
{
	m_mediaFrameSourceGroupType = HoloLensForCV::MediaFrameSourceGroupType::HoloLensResearchModeSensors;

	// Sensors to be enabled
	m_enabledSensorTypes.emplace_back(
		HoloLensForCV::SensorType::VisibleLightLeftFront);

	m_enabledSensorTypes.emplace_back(
		HoloLensForCV::SensorType::VisibleLightRightFront);

	m_mediaFrameSourceGroupStarted = false;
	m_isRunning = false;
	m_inProcess = false;
	m_handingRpcs = false;
	m_processedFrames = 0;
}

void ServerGRPC::Run(IBackgroundTaskInstance ^ taskInstance)
{
	if (!m_isRunning)
	{
		Serve();
	}
	if (!m_mediaFrameSourceGroupStarted)
	{
		StartHoloLensMediaFrameSourceGroup();
	}

	dbg::trace(L"Beginning main loop");

	// Main background loop to update sensors data
	while (m_isRunning)
	{
		// Update sensors data
		if (!m_inProcess)
		{
			Concurrency::create_task(StreamAsync()).then(
				[&]()
				{
					m_inProcess = false;
					if (m_processedFrames % 1 == 0)
						dbg::trace(L"Number of processed frames so far: %i", m_processedFrames);
					m_processedFrames++;
				});
		}
		// Handle RPC requests here
		if (!m_handingRpcs)
		{
			Concurrency::create_task(HandleRpcAsync()).then(
				[&]()
				{
					m_handingRpcs = false;
					dbg::trace(L"Handled Completion queue");
				});
		}
	}
}

void ServerGRPC::Serve()
{
	m_isRunning = true;
	m_streamerService.Run();
}

void ServerGRPC::Stop()
{
	if (m_isRunning)
		m_isRunning = false;
}

Windows::Foundation::IAsyncAction^ ServerGRPC::HandleRpcAsync()
{
	m_handingRpcs = true;
	return Concurrency::create_async(
		[&]()
		{
			return m_streamerService.HandleRpcs(m_sensorFrames);
		});
}

Concurrency::task<void> ServerGRPC::StreamAsync()
{
	dbg::TimerGuard timerGuard(
		L"ServerGRPC::StreamAsync",
		30.0 /* minimum_time_elapsed_in_milliseconds */);
	
	m_inProcess = true;

	Concurrency::task<void> streamTask = Concurrency::task_from_result();

	if (!m_mediaFrameSourceGroupStarted)
		return streamTask;

	m_sensorFrames.clear();
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
		m_sensorFrames.push_back(frame);
	}
	return streamTask;
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
		});
}

}
