#include "pch.h"
#include "StreamerServiceImpl.h"


StreamerServiceImpl::~StreamerServiceImpl()
{
	dbg::trace(L"Shutting down server");
	m_server->Shutdown();
	m_cq->Shutdown();
}

void StreamerServiceImpl::Run()
{
	//std::string server_address("0.0.0.0:50051");

	//ServerBuilder builder;
	//// Listener
	//builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
	//// Register service instance
	//builder.RegisterService(&m_service);
	//// Add completion queue to handle request asynchronously
	//m_cq = builder.AddCompletionQueue();
	//// Assemble server
	//m_server = builder.BuildAndStart();
	//dbg::trace(L"Server listening", server_address.c_str());

	//// Instantiate first CallData object to handle incoming requests
	//new CallData(&m_service, m_cq.get(), ENABLE);
	//new CallData(&m_service, m_cq.get(), INTRINSICS);
	//new CallData(&m_service, m_cq.get(), SENSORSTREAM);
}

void StreamerServiceImpl::HandleRpcs(std::vector<HoloLensForCV::SensorFrame^> sensorFrames)
{
	//void* tag;
	//bool ok;
	//// Check completion queue status for any pending request
	//GPR_ASSERT(m_cq->Next(&tag, &ok));
	//GPR_ASSERT(ok);
	//CallData* cd = static_cast<CallData*>(tag);
	//cd->UpdateSensorData(sensorFrames);
	//cd->Proceed();
}

//void CallData::Proceed()
//{
//	CameraIntrinsicsRPC camIntrinsics;
//	switch (m_status)
//	{
//	case CREATE:
//		m_status = PROCESS;
//		if (m_type == ENABLE)
//			m_service->RequestEnableSensors(&m_context, &m_enableRequest,
//				&m_responderEnable, m_cq, m_cq, this);
//		else if (m_type == INTRINSICS)
//			m_service->RequestGetCamIntrinsics(&m_context, &m_sensorRequest,
//				&m_responderIntrinsics, m_cq, m_cq, this);
//		else if (m_type == SENSORSTREAM)
//			m_service->RequestSensorStream(&m_context, &m_sensorRequest,
//				&m_writerSensorStreaming, m_cq, m_cq, this);
//		break;
//	case PROCESS:
//		new CallData(m_service, m_cq, m_type);
//
//		if (m_type == ENABLE)
//		{
//
//			m_responderEnable.Finish(m_enableRequest, grpc::Status::OK, this);
//		}
//		else if (m_type == INTRINSICS)
//		{
//			// Process request
//			dbg::trace(L"Processing getIntrinsics request");
//			camIntrinsics.set_fx(1);
//			camIntrinsics.set_fy(2);
//			camIntrinsics.set_cx(3);
//			camIntrinsics.set_cy(4);
//			// Response ready
//			m_responderIntrinsics.Finish(camIntrinsics, grpc::Status::OK, this);
//			m_status = FINISH;
//		}
//		else if (m_type == SENSORSTREAM)
//		{
//			if (m_sensorFrames.empty())
//			{
//				m_status = FINISH;
//				m_writerSensorStreaming.WriteAndFinish(SensorFrameRPC(),
//					m_writeOptions, grpc::Status::OK, this);
//				break;
//			}
//			HoloLensForCV::SensorType camera_name = ParseNameRPC(m_sensorRequest);
//			dbg::trace(L"Processing sensorStream request");
//			for (const auto sensorFrame : m_sensorFrames)
//			{
//				if (sensorFrame->FrameType == camera_name)
//				{
//					m_writerSensorStreaming.Write(MakeSensorFrameRPC(sensorFrame), this);
//					break;
//				}
//			}
//		}
//		break;
//	case FINISH:
//		dbg::trace(L"Deallocate CallData object");
//		delete this;
//		break;
//	default:
//		break;
//	}
//}