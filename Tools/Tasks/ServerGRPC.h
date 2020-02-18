#pragma once

#include "grpc/grpc.h"
#include <grpcpp/grpcpp.h>

#include "StreamerServiceImpl.h"

using namespace Windows::ApplicationModel::Background;

namespace Tasks {

public ref class ServerGRPC sealed : public IBackgroundTask
{
public:
	ServerGRPC();
	
	virtual void Run(IBackgroundTaskInstance^ taskInstance);

private:
	// gRPC Server
	void Serve();

	void Stop();

	Windows::Foundation::IAsyncAction^ HandleRpcAsync();

	StreamerServiceImpl m_streamerService;
	bool m_isRunning;

	// HoloLens sensor streaming
	Concurrency::task<void> StreamAsync();
	void StartHoloLensMediaFrameSourceGroup();

	HoloLensForCV::MediaFrameSourceGroupType m_mediaFrameSourceGroupType;
	HoloLensForCV::MediaFrameSourceGroup^ m_mediaFrameSourceGroup;
	bool m_mediaFrameSourceGroupStarted;
	HoloLensForCV::MultiFrameBuffer^ m_multiFrameBuffer;
	HoloLensForCV::SpatialPerception^ m_spatialPerception;
	std::vector<HoloLensForCV::SensorType> m_enabledSensorTypes;

	// Sensors data
	std::vector<HoloLensForCV::SensorFrame^> m_sensorFrames;

	bool m_inProcess;
	int m_processedFrames;
	bool m_handingRpcs;
};
}

