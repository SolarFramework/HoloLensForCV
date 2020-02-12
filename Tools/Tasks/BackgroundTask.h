#pragma once

#include "pch.h"

using namespace Windows::ApplicationModel::Background;
using namespace Windows::Storage;
using namespace Windows::System::Threading;
using namespace Windows::Foundation;

namespace Tasks
{
	[Windows::Foundation::Metadata::WebHostHidden]
	public ref class BackgroundTask sealed : public IBackgroundTask
	{
	public:
		
		virtual void Run(IBackgroundTaskInstance^ taskInstance);
		void OnCanceled(IBackgroundTaskInstance^ taskInstance, BackgroundTaskCancellationReason reason);
		//void OnCompleted(BackgroundTaskRegistration^ task, BackgroundTaskCompletedEventArgs^ args);

	private:

		Platform::Agile<BackgroundTaskDeferral> TaskDeferral = nullptr;
		bool keepRunning;
		
		void StartHoloLensMediaFrameSourceGroup();
		HoloLensForCV::MediaFrameSourceGroupType _selectedHoloLensMediaFrameSourceGroupType;
		HoloLensForCV::MediaFrameSourceGroup^ _holoLensMediaFrameSourceGroup;
		bool _holoLensMediaFrameSourceGroupStarted;
		HoloLensForCV::SpatialPerception^ _spatialPerception;
		HoloLensForCV::MultiFrameBuffer^ _multiFrameBuffer;
		long long _deltaTmin;
		
		Windows::Foundation::IAsyncAction^ RunAsync();
		concurrency::task<void> StreamAsync();
		boolean pairingInProgress;

		Windows::ApplicationModel::Background::BackgroundTaskCancellationReason CancelReason = Windows::ApplicationModel::Background::BackgroundTaskCancellationReason::Abort;
		volatile bool CancelRequested = false;

		Windows::System::Threading::ThreadPoolTimer^ PeriodicTimer = nullptr;
		unsigned int Progress = 0;
		Windows::ApplicationModel::Background::IBackgroundTaskInstance^ TaskInstance = nullptr;

		Windows::Media::Devices::Core::CameraIntrinsics^ leftCameraIntrinsics;
		Windows::Media::Devices::Core::CameraIntrinsics^ rightCameraIntrinsics;
		boolean inProcess;
		std::pair<uint8_t*, int32_t>  BackgroundTask::sensorFrameToImageBufferPair(HoloLensForCV::SensorFrame^ sensorFrame);

		struct HoloLensCameraCalibration
		{
			std::wstring SensorName;
			
			float FocalLengthX;
			float FocalLengthY;
			
			float PrincipalPointX;
			float PrincipalPointY;
			
			float RadialDistortionX;
			float RadialDistortionY;
			float RadialDistortionZ;
			
			float TangentialDistortionX;
			float TangentialDistortionY;
		};

		Platform::String^ matrixToString(Windows::Foundation::Numerics::float4x4 mat);

		//ROS handling

		void Advertise(Platform::String^ topic, Platform::String^ messageType);
		void PublishHello(Platform::String^ topic, Platform::String^ message);

		void Publish(Platform::String^ topic, Windows::Foundation::DateTime commonTimestamp,
			HoloLensForCV::SensorFrame^ leftFrame, Platform::Array<uint8_t>^ leftData, HoloLensForCV::SensorFrame^ rightFrame, Platform::Array<uint8_t>^ rightData);

		std::string base64_encode(unsigned char const*, unsigned int len);
		std::string base64_decode(std::string const& s);
		Platform::String^ StringFromAscIIChars(char* chars);
		Platform::String^ StringFromstd(std::string input);

		//Websocket handling
		bool busy;
		void SetBusy(bool value);

		void Connect();
		Concurrency::task<void> ConnectAsync();
		
		void Send(Platform::String^ message); void Send(Platform::Array<uint8_t>^ message); void Send(DateTime timestamp); //Publish
		Concurrency::task<void> SendAsync(Platform::String^ message); Concurrency::task<void> SendAsync(Platform::Array<uint8_t>^ message); Concurrency::task<void> SendAsync(DateTime timestamp);
		
		void OnDisconnect();
		
		Windows::Networking::Sockets::MessageWebSocket^ messageWebSocket;
		Windows::Storage::Streams::DataWriter^ messageWriter;
		void OnClosed(Windows::Networking::Sockets::IWebSocket^ sender, Windows::Networking::Sockets::WebSocketClosedEventArgs^ args);
		void CloseSocket();
	};
}
