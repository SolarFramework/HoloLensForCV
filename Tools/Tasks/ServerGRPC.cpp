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
		// Update sensors data for the gRPC app
		StreamAsync();
		m_streamerService.HandleRpcs(m_sensorFrames);
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

void ServerGRPC::StreamAsync()
{
	dbg::TimerGuard timerGuard(
		L"ServerGRPC::StreamAsync",
		30.0 /* minimum_time_elapsed_in_milliseconds */);
	
	m_inProcess = true;

	if (!m_mediaFrameSourceGroupStarted)
		return;

	m_sensorFrames.clear();
	int count = 0;
	// Read sensor frames
	for (const auto sensorType : m_enabledSensorTypes)
	{
		auto frame = m_multiFrameBuffer->GetLatestFrame(sensorType);
		if (frame == nullptr)
		{
			dbg::trace(L"Frame is null for sensor %s", sensorType.ToString()->Data());
			continue; // Skip this frame's processing, sensor not available
		}
		count++;
		m_sensorFrames.push_back(frame);
	}
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
		});
}

}
