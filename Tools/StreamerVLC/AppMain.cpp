//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#include "pch.h"
#include "AppMain.h"

using namespace Windows::Foundation;
using namespace Windows::Foundation::Numerics;
using namespace Windows::Networking;
using namespace Windows::Networking::Connectivity;
using namespace Windows::Networking::Sockets;
using namespace Windows::Storage::Streams;
using namespace Windows::Graphics::Holographic;
using namespace Windows::Perception::Spatial;
using namespace Windows::UI::Input::Spatial;
using namespace std::placeholders;
using namespace Windows::ApplicationModel::Background;
using namespace Windows::UI::Core;
using namespace Tasks;

namespace StreamerVLC
{
	// Refer to Windows Universal Samples for UWP apps, segment on Background Tasks
	void AppMain::InitializeBackgroundStreamer()
	{
		BackgroundExecutionManager::RequestAccessAsync();

		auto iter = BackgroundTaskRegistration::AllTasks->First();
		auto hascur = iter->HasCurrent;
		// Check if any background task still remains from previous instances of the app
		while (hascur)
		{
			auto cur = iter->Current->Value;
			if (cur->Name == m_taskName)
			{
				m_taskRegistered = true;
				dbg::trace(L"Background Task already still registered!");
				break;
			}
			hascur = iter->MoveNext();
		}
		// Unregister any previous instances of the app
		if (m_taskRegistered)
		{
			// Loop through all ungrouped background tasks and unregister any with the name passed into this function.
			for (auto pair : BackgroundTaskRegistration::AllTasks)
			{
				auto task = pair->Value;
				if (task->Name == m_taskName)
				{
					task->Unregister(true);
				}
			}
			dbg::trace(L"Background Task deregistered!");
			m_taskRegistered = false;
		}
		// Register a brand new background task
		{
			auto builder = ref new BackgroundTaskBuilder();

			builder->Name = m_taskName;
			builder->TaskEntryPoint = "Tasks.ServerGRPC";
			
			ApplicationTrigger^ trigger = ref new ApplicationTrigger();
			builder->SetTrigger(trigger);

			BackgroundTaskRegistration^ task = builder->Register();
			dbg::trace(L"Task registered!");

			concurrency::create_task(trigger->RequestAsync()).then(
				[this, trigger](concurrency::task<ApplicationTriggerResult> applicationTriggerTask)
				{
					ApplicationTriggerResult result = applicationTriggerTask.get();
					auto res = "App Trigger Result = " + result.ToString();
					dbg::trace(L"Task triggered!");
					dbg::trace(result.ToString()->Data());
				});
		}
	}

	// Loads and initializes application assets when the application is loaded.
	AppMain::AppMain(const std::shared_ptr<Graphics::DeviceResources>& deviceResources) :
		Holographic::AppMainBase(deviceResources),
		m_taskRegistered(false)
	{ }

	void AppMain::OnHolographicSpaceChanged(
		Windows::Graphics::Holographic::HolographicSpace^ holographicSpace)
	{
		InitializeBackgroundStreamer();
	}

	void AppMain::OnSpatialInput(
		_In_ Windows::UI::Input::Spatial::SpatialInteractionSourceState^ pointerState)
	{ }

	// Updates the application state once per frame.
	void AppMain::OnUpdate(
		_In_ Windows::Graphics::Holographic::HolographicFrame^ holographicFrame,
		_In_ const Graphics::StepTimer& stepTimer)
	{ }

	void AppMain::OnPreRender()
	{ }

	// Renders the current frame to each holographic camera, according to the
	// current application and spatial positioning state.
	void AppMain::OnRender()
	{ }

	// Notifies classes that use Direct3D device resources that the device resources
	// need to be released before this method returns.
	void AppMain::OnDeviceLost()
	{
		if (m_taskRegistered)
		{
			// Loop through all ungrouped background tasks and unregister any with the name passed into this function.
			for (auto pair : BackgroundTaskRegistration::AllTasks)
			{
				auto task = pair->Value;
				if (task->Name == m_taskName)
				{
					task->Unregister(true);
				}
			}
			dbg::trace(L"Background Task deregistered!");
			m_taskRegistered = false;
		}
	}

	// Notifies classes that use Direct3D device resources that the device resources
	// may now be recreated.
	void AppMain::OnDeviceRestored()
	{ }

	// Called when the application is suspending.
	void AppMain::SaveAppState()
	{ }

	// Called when the application is resuming.
	void AppMain::LoadAppState()
	{ 
		// Reset backgrounder streaming server.
		InitializeBackgroundStreamer();
	}
}
