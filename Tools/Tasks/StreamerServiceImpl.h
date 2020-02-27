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


