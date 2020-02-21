#pragma once

#include <grpc/grpc.h>
#include <grpcpp/grpcpp.h>
#include <grpc/support/log.h>

#include "sensorStreaming.grpc.pb.h"

using grpc::Server;
using grpc::ServerAsyncResponseWriter;
using grpc::ServerAsyncWriter;
using grpc::ServerAsyncReaderWriter;
using grpc::ServerWriter;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerCompletionQueue;
using sensorStreaming::Streamer;
using sensorStreaming::SensorListRPC;
using sensorStreaming::NameRPC;
using sensorStreaming::MatRPC;
using sensorStreaming::CameraIntrinsicsRPC;
using sensorStreaming::PoseRPC;
using sensorStreaming::ImageRPC;
using sensorStreaming::SensorFrameRPC;

enum RequestType { ENABLE, INTRINSICS, SENSORSTREAM };
enum CallStatus { CREATE, PROCESS, FINISH };

class StreamerServiceImpl final
{
public:
	~StreamerServiceImpl();

	void Run();

	void HandleRpcs(std::vector<HoloLensForCV::SensorFrame^> sensorFrames);

private:
	std::unique_ptr<ServerCompletionQueue> m_cq;
	Streamer::AsyncService m_service;
	std::unique_ptr<Server> m_server;
};


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
	{
		Proceed();
	}

	void Proceed();

	void UpdateSensorData(std::vector<HoloLensForCV::SensorFrame^> sensorFrames)
	{
		m_sensorFrames = sensorFrames;
	}

	CallStatus getStatus() { return m_status; }

	RequestType getType() { return m_type;  }

private:
	Streamer::AsyncService* m_service;
	ServerCompletionQueue* m_cq;
	ServerContext m_context;

	SensorListRPC m_enableRequest;
	NameRPC m_sensorRequest;
	CameraIntrinsicsRPC m_replyIntrinsics;
	SensorFrameRPC m_replySensorStreaming;

	ServerAsyncResponseWriter<CameraIntrinsicsRPC> m_responderIntrinsics;
	ServerAsyncWriter<SensorFrameRPC> m_writerSensorStreaming;
	ServerAsyncResponseWriter<SensorListRPC> m_responderEnable;

	grpc::WriteOptions m_writeOptions;

	CallStatus m_status;
	RequestType m_type;

	std::vector<HoloLensForCV::SensorFrame^> m_sensorFrames;
};