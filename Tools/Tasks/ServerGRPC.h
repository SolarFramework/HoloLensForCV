#pragma once

#include "grpc/grpc.h"
#include <grpcpp/grpcpp.h>

using namespace Windows::ApplicationModel::Background;

namespace Tasks {

public ref class ServerGRPC sealed : public IBackgroundTask
{
public:
	ServerGRPC();
	
	virtual void Run(IBackgroundTaskInstance^ taskInstance);

	//HoloLensForCV::SensorType FromStringNameToSensorType(std::string sensorName);
private:
	// gRPC handling
	void Serve();
	void Stop();

	grpc::ServerBuilder m_serverBuilder;
	std::unique_ptr<grpc::Server> m_server;

	std::string m_serverAddress;
	bool m_isRunning;

	// HoloLens streaming
	Concurrency::task<void> StreamAsync();
	void StartHoloLensMediaFrameSourceGroup();
	HoloLensForCV::MediaFrameSourceGroupType m_mediaFrameSourceGroupType;
	HoloLensForCV::MediaFrameSourceGroup^ m_mediaFrameSourceGroup;
	bool m_mediaFrameSourceGroupStarted;
	HoloLensForCV::MultiFrameBuffer^ m_multiFrameBuffer;
	HoloLensForCV::SpatialPerception^ m_spatialPerception;
	std::vector<HoloLensForCV::SensorType> m_enabledSensorTypes;

	bool m_inProcess;
	int m_processedFrames;
};

}

