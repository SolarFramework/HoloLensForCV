#pragma once

#include <grpc/grpc.h>
#include <grpcpp/grpcpp.h>

#include "sensorStreaming.grpc.pb.h"

using grpc::Server;
using grpc::ServerAsyncResponseWriter;
using grpc::ServerAsyncWriter;
using grpc::ServerAsyncReaderWriter;
using grpc::ServerWriter;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerCompletionQueue;
using grpc::WriteOptions;
using sensorStreaming::Streamer;
using sensorStreaming::SensorListRPC;
using sensorStreaming::NameRPC;
using sensorStreaming::MatRPC;
using sensorStreaming::CameraIntrinsicsRPC;
using sensorStreaming::PoseRPC;
using sensorStreaming::ImageRPC;
using sensorStreaming::SensorFrameRPC;

using namespace Windows::ApplicationModel::Background;


namespace Tasks {

enum RequestType { ENABLE, INTRINSICS, SENSORSTREAM };
enum CallStatus { CREATE, PROCESS, FINISH };

public ref class ServerGRPC sealed : public IBackgroundTask
{
public:
	ServerGRPC();
	
	virtual void Run(IBackgroundTaskInstance^ taskInstance);

private:
	class CallData
	{
	public:
		CallData(Streamer::AsyncService* service, ServerCompletionQueue* cq, RequestType rt) :
			m_service(service),
			m_cq(cq),
			m_responderEnable(&m_context),
			m_responderIntrinsics(&m_context),
			m_writerSensorStreaming(&m_context),
			m_status(CREATE),
			m_type(rt)
		{ }

		void RegisterRequest()
		{
			setStatus(PROCESS);
			if (m_type == ENABLE)
				m_service->RequestEnableSensors(&m_context, &m_enableRequest,
					&m_responderEnable, m_cq, m_cq, this);
			else if (m_type == INTRINSICS)
				m_service->RequestGetCamIntrinsics(&m_context, &m_sensorRequest,
					&m_responderIntrinsics, m_cq, m_cq, this);
			else if (m_type == SENSORSTREAM)
				m_service->RequestSensorStream(&m_context, &m_sensorRequest,
					&m_writerSensorStreaming, m_cq, m_cq, this);
		}

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

		Streamer::AsyncService* m_service;
		ServerCompletionQueue* m_cq;
		ServerContext m_context;
		WriteOptions m_writeOptions;
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

	bool m_isRunning;
	Streamer::AsyncService m_service;
	std::unique_ptr<ServerCompletionQueue> m_cq;
	std::unique_ptr<Server> m_server;

	HoloLensForCV::MediaFrameSourceGroupType m_mediaFrameSourceGroupType;
	HoloLensForCV::MediaFrameSourceGroup^ m_mediaFrameSourceGroup;
	bool m_mediaFrameSourceGroupStarted;
	HoloLensForCV::MultiFrameBuffer^ m_multiFrameBuffer;
	HoloLensForCV::SpatialPerception^ m_spatialPerception;
	std::vector<HoloLensForCV::SensorType> m_enabledSensorTypes;

	// Sensors data
	std::vector<HoloLensForCV::SensorFrame^> m_sensorFrames;
	HoloLensForCV::SensorFrame^ m_currentFrame;

	int m_count;
};

}
