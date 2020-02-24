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
	class CallData
	{
	public:
		CallData(ServerGRPC^ server, RequestType rt) :
			m_responderEnable(&m_context),
			m_responderIntrinsics(&m_context),
			m_writerSensorStreaming(&m_context),
			m_status(CREATE),
			m_type(rt)
		{}

		CallStatus getStatus() { return m_status; }

		RequestType getType() { return m_type; }

		void setStatus(CallStatus status) { m_status = status; }

		SensorListRPC m_enableRequest;
		NameRPC m_sensorRequest;
		CameraIntrinsicsRPC m_replyIntrinsics;
		SensorFrameRPC m_replySensorStreaming;

		ServerAsyncResponseWriter<CameraIntrinsicsRPC> m_responderIntrinsics;
		ServerAsyncWriter<SensorFrameRPC> m_writerSensorStreaming;
		ServerAsyncResponseWriter<SensorListRPC> m_responderEnable;

		ServerContext m_context;
		grpc::WriteOptions m_writeOptions;
	private:
		CallStatus m_status;
		RequestType m_type;
	};

	// gRPC Server
	void Serve();

	void HandleRpcs();

	void Proceed(CallData * cd);

	void Stop();

	void StreamAsync();

	StreamerServiceImpl m_streamerService;
	bool m_isRunning;
	Streamer::AsyncService m_service;
	std::unique_ptr<ServerCompletionQueue> m_cq;
	std::unique_ptr<Server> m_server;

	// HoloLens sensor streaming
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
