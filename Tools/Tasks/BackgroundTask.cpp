#include "pch.h"
#include "BackgroundTask.h"

#include <locale>
#include <codecvt>
#include <string>

using namespace Tasks;
using namespace dbg;
using namespace concurrency;
using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Networking::Sockets;
using namespace Windows::Security::Cryptography::Certificates;
using namespace Windows::Storage::Streams;
using namespace Windows::UI::Core;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Navigation;
using namespace Windows::Web;
using namespace Windows::Data::Json;
using namespace Windows::Security::Cryptography;
using namespace Windows::Globalization::DateTimeFormatting;


namespace Tasks {
static int count = 0;
static long long s_previousTimestamp = 0;
static int id;
static int pubCount;
static bool connected = false;

static const std::string base64_chars =
"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyz"
"0123456789+/";

static const Platform::String^ base64_charsp =
"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyz"
"0123456789+/";

static inline bool is_base64(unsigned char c)
{
	return (isalnum(c) || (c == '+') || (c == '/'));
}

void BackgroundTask::Run(IBackgroundTaskInstance^ taskInstance)
{
	dbg::trace(L"Background Task::Run called");
	
	id = 0;
	pubCount = 0;
	_selectedHoloLensMediaFrameSourceGroupType = HoloLensForCV::MediaFrameSourceGroupType::HoloLensResearchModeSensors;
	_holoLensMediaFrameSourceGroupStarted = false;
	keepRunning = true;
	auto cost = BackgroundWorkCost::CurrentBackgroundWorkCost;
	auto settings = ApplicationData::Current->LocalSettings;
	settings->Values->Insert("BackgroundWorkCost", cost.ToString());
	pairingInProgress = false;
	taskInstance->Canceled += ref new BackgroundTaskCanceledEventHandler(this, &BackgroundTask::OnCanceled);
	
	TaskDeferral = taskInstance->GetDeferral();
	TaskInstance = taskInstance;

	_deltaTmin = 400000;

	Connect();
	
	inProcess = false;
	while (keepRunning)
	{
		if (!_holoLensMediaFrameSourceGroupStarted)
		{
			if (!inProcess) 
			{ 	
				StartHoloLensMediaFrameSourceGroup(); 
			}
		}				
		else if (_holoLensMediaFrameSourceGroupStarted && !pairingInProgress)
		{
			concurrency::create_task(BackgroundTask::RunAsync()).then([&]()
			{
				pairingInProgress = false;
				count++;
			});
		}	
	}
	//TO-DO, send an output indicating that the bg task is closing

	dbg::trace(L"Background Task Completed.");

	TaskDeferral->Complete();
}

void BackgroundTask::StartHoloLensMediaFrameSourceGroup()
{
	std::vector<HoloLensForCV::SensorType> enabledSensorTypes;
	inProcess = true;
	// Enabling all of the Research Mode sensors at the same time can be quite expensive
	// performance-wise. It's best to scope down the list of enabled sensors to just those
	// that are required for a given task. 

	// In this example, selected: the visible light front cameras.

	enabledSensorTypes.emplace_back(
		HoloLensForCV::SensorType::VisibleLightLeftFront);

	enabledSensorTypes.emplace_back(
		HoloLensForCV::SensorType::VisibleLightRightFront);

	_multiFrameBuffer = ref new HoloLensForCV::MultiFrameBuffer();

	_spatialPerception = ref new HoloLensForCV::SpatialPerception();

	_holoLensMediaFrameSourceGroup =
		ref new HoloLensForCV::MediaFrameSourceGroup(
			_selectedHoloLensMediaFrameSourceGroupType,
			_spatialPerception,
			_multiFrameBuffer);

	for (const auto enabledSensorType : enabledSensorTypes)
	{
		_holoLensMediaFrameSourceGroup->Enable(
			enabledSensorType);
	}
	dbg::trace(L"calling startasync for mediaframesourcegroup");
	concurrency::create_task(_holoLensMediaFrameSourceGroup->StartAsync()).then(
		[&]()
		{
			dbg::trace(L"media frame source group started.");
			inProcess = false;
			_holoLensMediaFrameSourceGroupStarted = true;

			leftCameraIntrinsics = _holoLensMediaFrameSourceGroup->GetCameraIntrinsics(HoloLensForCV::SensorType::VisibleLightLeftFront);
			rightCameraIntrinsics = _holoLensMediaFrameSourceGroup->GetCameraIntrinsics(HoloLensForCV::SensorType::VisibleLightRightFront);

			if (leftCameraIntrinsics == nullptr)
			{
				dbg::trace(L"Camera Intrinsics nullptr.");	// Not calibrated
			}
		});
}

Windows::Foundation::IAsyncAction^ BackgroundTask::RunAsync()
{
	return concurrency::create_async(
		[this]()
		{
			return StreamAsync();
		});
}

concurrency::task<void> BackgroundTask::StreamAsync()
{
	Concurrency::task<void> streamTask =
		Concurrency::task_from_result();

	pairingInProgress = true;
	const float c_timestampTolerance = 0.001f;

	auto commonTime = _multiFrameBuffer->GetTimestampForSensorPair(
		HoloLensForCV::SensorType::VisibleLightLeftFront,
		HoloLensForCV::SensorType::VisibleLightRightFront,
		c_timestampTolerance);
	
	if (_multiFrameBuffer->GetFrameForTime(HoloLensForCV::SensorType::VisibleLightLeftFront,  commonTime, c_timestampTolerance) == nullptr ||
	    _multiFrameBuffer->GetFrameForTime(HoloLensForCV::SensorType::VisibleLightRightFront, commonTime, c_timestampTolerance) == nullptr)
	{
		return streamTask; // Can't retrieve frame from sensors (spatial localization not available)
	}
	HoloLensForCV::SensorFrame^ leftFrame;
	HoloLensForCV::SensorFrame^ rightFrame;
	try
	{
		leftFrame = _multiFrameBuffer->GetFrameForTime(
			HoloLensForCV::SensorType::VisibleLightLeftFront,
			commonTime,
			c_timestampTolerance);

		rightFrame = _multiFrameBuffer->GetFrameForTime(
			HoloLensForCV::SensorType::VisibleLightRightFront,
			commonTime,
			c_timestampTolerance);

		if (leftFrame == nullptr || rightFrame == nullptr)
		{
			return streamTask;
		}

		// Test synchronization between sensors
		auto timeDiff100ns = leftFrame->Timestamp.UniversalTime - rightFrame->Timestamp.UniversalTime;
		if (std::abs(timeDiff100ns * 1e-7f) > 2e-3f)
		{
			dbg::trace(L"StreamAsync: times are different by %f seconds", std::abs(timeDiff100ns * 1e-7f));
		}

		// Prevent overfeeding the network connection
		if (commonTime.UniversalTime - s_previousTimestamp < _deltaTmin)
		{
			return streamTask;
		}

		s_previousTimestamp = commonTime.UniversalTime;
		// TODO: code to retrieve Array<uint8:t>^ needs to be checked 
		auto outPair = std::make_pair(leftFrame, rightFrame);
		std::pair<uint8_t*, int32_t> leftData = sensorFrameToImageBufferPair(outPair.first);
		std::pair<uint8_t*, int32_t> rightData = sensorFrameToImageBufferPair(outPair.second);
		Platform::Array<uint8_t>^ imageBufferAsPlatformArrayLeft;
		Platform::Array<uint8_t>^ imageBufferAsPlatformArrayRight;

		imageBufferAsPlatformArrayLeft =
			ref new Platform::Array<uint8_t>(
				leftData.first,
				leftData.second);
		imageBufferAsPlatformArrayRight =
			ref new Platform::Array<uint8_t>(
				rightData.first,
				rightData.second);

		if (connected)
		{
			Publish("/hololens", commonTime, leftFrame, imageBufferAsPlatformArrayLeft, rightFrame, imageBufferAsPlatformArrayRight);
			pubCount++;
		}
	}
	catch (Exception^ e)
	{
		dbg::trace(L"StreamAsync: Exception.");
		return streamTask;
	}
	return streamTask;
}

Platform::String^ BackgroundTask::matrixToString(Windows::Foundation::Numerics::float4x4 mat)
{
	Platform::String^ s = mat.m11.ToString() + "," + mat.m12.ToString() + "," + mat.m13.ToString() + "," + mat.m14.ToString() + "," +
						  mat.m21.ToString() + "," + mat.m22.ToString() + "," + mat.m23.ToString() + "," + mat.m24.ToString() + "," +
						  mat.m31.ToString() + "," + mat.m32.ToString() + "," + mat.m33.ToString() + "," + mat.m34.ToString() + "," +
						  mat.m41.ToString() + "," + mat.m42.ToString() + "," + mat.m43.ToString() + "," + mat.m44.ToString();
	return s;
}

std::pair<uint8_t*, int32_t>  BackgroundTask::sensorFrameToImageBufferPair(HoloLensForCV::SensorFrame^ sensorFrame)
{
	Windows::Graphics::Imaging::SoftwareBitmap^ bitmap;
	Windows::Graphics::Imaging::BitmapBuffer^ bitmapBuffer;
	Windows::Foundation::IMemoryBufferReference^ bitmapBufferReference;

	int32_t imageWidth = 0;
	int32_t imageHeight = 0;
	int32_t pixelStride = 1;
	int32_t rowStride = 0;

	Platform::Array<uint8_t>^ imageBufferAsPlatformArray;
	int32_t imageBufferSize = 0;

	{
#if DBG_ENABLE_INFORMATIONAL_LOGGING
		dbg::TimerGuard timerGuard(
			L"AppMain::sensorFrameToImageBuffer: buffer preparation",
			4.0 /* minimum_time_elapsed_in_milliseconds */);
#endif /* DBG_ENABLE_INFORMATIONAL_LOGGING */

		bitmap = sensorFrame->SoftwareBitmap;

		imageWidth = bitmap->PixelWidth;
		imageHeight = bitmap->PixelHeight;

		bitmapBuffer = bitmap->LockBuffer(
				Windows::Graphics::Imaging::BitmapBufferAccessMode::Read);

		bitmapBufferReference = bitmapBuffer->CreateReference();

		uint32_t bitmapBufferDataSize = 0;

		uint8_t* bitmapBufferData =
			Io::GetTypedPointerToMemoryBuffer<uint8_t>(
				bitmapBufferReference,
				bitmapBufferDataSize);

		switch (bitmap->BitmapPixelFormat)
		{
			case Windows::Graphics::Imaging::BitmapPixelFormat::Bgra8:
				pixelStride = 4;
				break;

			case Windows::Graphics::Imaging::BitmapPixelFormat::Gray16:
				pixelStride = 2;
				break;

			case Windows::Graphics::Imaging::BitmapPixelFormat::Gray8:
				pixelStride = 1;
				break;

			default:
#if DBG_ENABLE_INFORMATIONAL_LOGGING
				dbg::trace(
					L"AppMain::sensorFrameToImageBuffer: unrecognized bitmap pixel format, assuming 1 byte per pixel");
#endif /* DBG_ENABLE_INFORMATIONAL_LOGGING */
				break;
		}
		rowStride = imageWidth * pixelStride;
		imageBufferSize = imageHeight * rowStride;

		ASSERT(imageBufferSize == (int32_t)bitmapBufferDataSize);

		std::pair<uint8_t*, int32_t> outPair = std::make_pair(bitmapBufferData, imageBufferSize);

		return outPair;
	}
}

void BackgroundTask::OnCanceled(IBackgroundTaskInstance^ taskInstance, BackgroundTaskCancellationReason reason)
{
	CancelRequested = true;
	CancelReason = reason;
}

void BackgroundTask::SetBusy(bool value)
{
	busy = value;
}

void BackgroundTask::Connect()
{
	SetBusy(true);
	ConnectAsync().then(
		[this]()
		{
			SetBusy(false);
			connected = true;

			Advertise("/listener", "std_msgs/String");
			Advertise("/hololens", "project/HoloLensStereo");
			PublishHello("/listener", "Hello from the HoloLens.");
		});
}


task<void> BackgroundTask::ConnectAsync()
{
	Uri^ server = ref new Uri("ws://10.10.254.226:9090/");
	if (!server)
	{
		dbg::trace(L"Uri false");
		return task_from_result();
	}
	messageWebSocket = ref new MessageWebSocket();
	messageWebSocket->Control->MessageType = SocketMessageType::Utf8;

	return create_task(messageWebSocket->ConnectAsync(server))
		.then([this](task<void> previousTask)
	{
		try
		{
			// Reraise any exception that occurred in the task.
			previousTask.get();
			dbg::trace(L"web socket connected.");
		}
		catch (Exception^ ex)
		{
			// Error happened during connect operation.
			delete messageWebSocket;
			messageWebSocket = nullptr;
			dbg::trace(L"ConnectAsync: Error during connection operation.");
			return;
		}
		// The default DataWriter encoding is Utf8.
		dbg::trace(L"ConnectAsync: Connected.");
		messageWriter = ref new DataWriter(messageWebSocket->OutputStream);
		dbg::trace(L"ConnectAsync: DataWriter initialized.");
	});
}

void BackgroundTask::Send(Platform::String^ message)
{
	SetBusy(true);
	SendAsync(message).then(
		[this]()
		{
			SetBusy(false);
		});
}

void BackgroundTask::Send(Platform::Array<uint8_t>^ message)
{
	SetBusy(true);
	SendAsync(message).then(
		[this]()
		{
			SetBusy(false);
		});
}

void BackgroundTask::Send(DateTime timestamp)
{
	SetBusy(true);
	SendAsync(timestamp).then(
		[this]()
		{
			SetBusy(false);
		});
}

task<void> BackgroundTask::SendAsync(Platform::String^ message) {

	auto a = messageWriter->WriteString(message);
	//dbg::trace(L"SendAsync: %i string length, in bytes. ", a);
	return concurrency::create_task(messageWriter->StoreAsync()).then(
		[this](task<unsigned int> previousTask)
		{
			previousTask.get();
			//dbg::trace(L"Send Complete");
		});
}

task<void> BackgroundTask::SendAsync(Platform::Array<uint8_t>^ message) {

	messageWriter->WriteBytes(message->Value);
	return concurrency::create_task(messageWriter->StoreAsync()).then(
		[this](task<unsigned int> previousTask)
		{
			previousTask.get();
			//dbg::trace(L"Send Complete");
		});
}

task<void> BackgroundTask::SendAsync(DateTime timestamp)
{
	messageWriter->WriteDateTime(timestamp);
	return concurrency::create_task(messageWriter->StoreAsync()).then(
		[this](task<unsigned int> previousTask)
		{
			previousTask.get();
			//dbg::trace(L"Send Complete");
		});
}

void BackgroundTask::Advertise(Platform::String^ topic, Platform::String^ messageType)
{
	JsonObject^ jsonObject = ref new JsonObject();
	jsonObject->Insert("op", JsonValue::CreateStringValue("advertise"));
	jsonObject->Insert("topic", JsonValue::CreateStringValue(topic));
	jsonObject->Insert("type", JsonValue::CreateStringValue(messageType));

	String^ output = jsonObject->Stringify();

	dbg::trace(L"Advertise: %s", output->Data());
	Send(output);
}

void BackgroundTask::Publish(Platform::String^ topic, Windows::Foundation::DateTime commonTimestamp, HoloLensForCV::SensorFrame^ leftFrame, Platform::Array<uint8_t>^ leftData, HoloLensForCV::SensorFrame^ rightFrame, Platform::Array<uint8_t>^ rightData)
{
	JsonObject^ jsonObject = ref new JsonObject();
		
	// op and topic
	jsonObject->Insert("op", JsonValue::CreateStringValue("publish"));
	jsonObject->Insert("topic", JsonValue::CreateStringValue(topic));

	JsonObject^ msg = ref new JsonObject();
	msg->Insert("timestamp", JsonValue::CreateNumberValue(commonTimestamp.UniversalTime));
	msg->Insert("height" ,JsonValue::CreateNumberValue(leftFrame->SensorStreamingCameraIntrinsics->ImageHeight));
	msg->Insert("width", JsonValue::CreateNumberValue(leftFrame->SensorStreamingCameraIntrinsics->ImageWidth));

	// Image buffer data as base64 encoded string. 
	Platform::String^ leftString = StringFromstd(base64_encode(leftData->Data, leftData->Length));

	// CryptographicBuffer::EncodeToBase64String(CryptographicBuffer::CreateFromByteArray(leftData)); UWP JSON encrypting. Can be useful if data being received by another UWP app. 
	Platform::String^ rightString = StringFromstd(base64_encode(rightData->Data, rightData->Length));
	// CryptographicBuffer::EncodeToBase64String(CryptographicBuffer::CreateFromByteArray(rightData));

	msg->Insert("left64", JsonValue::CreateStringValue(leftString));
	msg->Insert("right64", JsonValue::CreateStringValue(rightString));

	// Left frame matrices
	msg->Insert("leftFrameToOrigin", JsonValue::CreateStringValue(matrixToString(leftFrame->FrameToOrigin)));
	msg->Insert("leftCameraViewTransform", JsonValue::CreateStringValue(matrixToString(leftFrame->CameraViewTransform)));
	msg->Insert("leftCameraProjectionTransform", JsonValue::CreateStringValue(matrixToString(leftFrame->CameraProjectionTransform)));
	// Right frame matrices
	msg->Insert("rightFrameToOrigin", JsonValue::CreateStringValue(matrixToString(rightFrame->FrameToOrigin)));
	msg->Insert("rightCameraViewTransform", JsonValue::CreateStringValue(matrixToString(rightFrame->CameraViewTransform)));
	msg->Insert("rightCameraProjectionTransform", JsonValue::CreateStringValue(matrixToString(rightFrame->CameraProjectionTransform)));

	// Final Json publish
	jsonObject->Insert("msg", JsonValue::Parse( msg->Stringify() ));
	Send(jsonObject->Stringify());
}

void BackgroundTask::PublishHello(Platform::String^ topic, Platform::String^ message)
{
	JsonObject^ jsonObject = ref new JsonObject();
	jsonObject->Insert("op", JsonValue::CreateStringValue("publish"));
	jsonObject->Insert("topic", JsonValue::CreateStringValue(topic));

	JsonObject^ dataX = ref new JsonObject();
	dataX->Insert("data", JsonValue::CreateStringValue(message));
		
	jsonObject->Insert("msg", JsonValue::Parse(dataX->Stringify()));

	Send(jsonObject->Stringify());
}

void BackgroundTask::OnDisconnect()
{
	SetBusy(true);
	CloseSocket();
	SetBusy(false);
}

void BackgroundTask::OnClosed(IWebSocket^ sender, WebSocketClosedEventArgs^ args)
{
	if (messageWebSocket == sender)
	{
		CloseSocket();
	}	
}

void BackgroundTask::CloseSocket()
{
	if (messageWriter != nullptr)
	{
		messageWriter->DetachStream();
		delete messageWriter;
		messageWriter = nullptr;
	}
	if (messageWebSocket != nullptr)
	{
		messageWebSocket->Close(1000, "Closed.");
		messageWebSocket = nullptr;
	}
}

std::string BackgroundTask::base64_encode(unsigned char const* bytes_to_encode, unsigned int in_len)
{
	std::string ret;
	int i = 0;
	int j = 0;
	unsigned char char_array_3[3];
	unsigned char char_array_4[4];

	while (in_len--) {
		char_array_3[i++] = *(bytes_to_encode++);
		if (i == 3) {
			char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
			char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
			char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
			char_array_4[3] = char_array_3[2] & 0x3f;

			for (i = 0; (i < 4); i++)
				ret += base64_chars[char_array_4[i]];
			i = 0;
		}
	}
	if (i)
	{
		for (j = i; j < 3; j++)
			char_array_3[j] = '\0';

		char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
		char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
		char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);

		for (j = 0; (j < i + 1); j++)
			ret += base64_chars[char_array_4[j]];

		while ((i++ < 3))
			ret += '=';
	}
	return ret;
}

std::string BackgroundTask::base64_decode(std::string const& encoded_string)
{
	size_t in_len = encoded_string.size();
	int i = 0;
	int j = 0;
	int in_ = 0;
	unsigned char char_array_4[4], char_array_3[3];
	std::string ret;

	while (in_len-- && (encoded_string[in_] != '=') && is_base64(encoded_string[in_])) {
		char_array_4[i++] = encoded_string[in_]; in_++;
		if (i == 4) {
			for (i = 0; i < 4; i++)
				char_array_4[i] = base64_chars.find(char_array_4[i]) & 0xff;

			char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
			char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
			char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

			for (i = 0; (i < 3); i++)
				ret += char_array_3[i];
			i = 0;
		}
	}
	if (i) {
		for (j = 0; j < i; j++)
			char_array_4[j] = base64_chars.find(char_array_4[j]) & 0xff;

		char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
		char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);

		for (j = 0; (j < i - 1); j++) ret += char_array_3[j];
	}
	return ret;
}

String^ BackgroundTask::StringFromstd(std::string input)
{
	std::wstring wid_str = std::wstring(input.begin(), input.end());
	const wchar_t* w_char = wid_str.c_str();
	Platform::String^ p_string = ref new Platform::String(w_char);
	return p_string;
}

String^ BackgroundTask::StringFromAscIIChars(char* chars)
{
	size_t newsize = strlen(chars) + 1;
	wchar_t * wcstring = new wchar_t[newsize];
	size_t convertedChars = 0;
	mbstowcs_s(&convertedChars, wcstring, newsize, chars, _TRUNCATE);
	String^ str = ref new Platform::String(wcstring);
	delete[] wcstring;
	return str;
}

} // namespace Tasks