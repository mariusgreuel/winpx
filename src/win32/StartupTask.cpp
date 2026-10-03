//
// StartupTask.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include <pch.h>

#include "StartupTask.h"
#include "StartupTask.tmh"

#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.Foundation.h>

using namespace winrt;
using namespace winrt::Windows;

namespace win32
{
    static Foundation::IAsyncOperation<ApplicationModel::StartupTaskState> GetStateAsync(hstring taskName)
    {
        try
        {
            auto startupTask = co_await ApplicationModel::StartupTask::GetAsync(taskName);
            co_return startupTask.State();
        }
        catch (const winrt::hresult_error& e)
        {
            DoTraceMessage(WppWarning, "Failed to get startup task state: %s", to_string(e.message()).c_str());
            co_return ApplicationModel::StartupTaskState::Disabled;
        }
    }

    static fire_and_forget EnableAsync(hstring taskName, bool enable)
    {
        try
        {
            auto startupTask = co_await ApplicationModel::StartupTask::GetAsync(taskName);

            if (enable)
            {
                if (startupTask.State() != ApplicationModel::StartupTaskState::Enabled)
                {
                    auto newState = co_await startupTask.RequestEnableAsync();
                    if (newState == ApplicationModel::StartupTaskState::DisabledByUser)
                    {
                        DoTraceMessage(WppVerbose, "DisabledByUser");
                    }
                }
            }
            else
            {
                if (startupTask.State() == ApplicationModel::StartupTaskState::Enabled)
                {
                    startupTask.Disable();
                }
            }
        }
        catch (const winrt::hresult_error& e)
        {
            DoTraceMessage(WppWarning, "Failed to enable startup task: %s", to_string(e.message()).c_str());
        }
    }

    bool StartupTask::IsEnabled(const char* taskName)
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: taskName=%s", taskName);

        auto state = GetStateAsync(to_hstring(taskName)).get();
        return state == ApplicationModel::StartupTaskState::Enabled ||
               state == ApplicationModel::StartupTaskState::EnabledByPolicy;
    }

    bool StartupTask::IsDisabledByUser(const char* taskName)
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: taskName=%s", taskName);

        auto state = GetStateAsync(to_hstring(taskName)).get();
        return state == ApplicationModel::StartupTaskState::DisabledByUser ||
               state == ApplicationModel::StartupTaskState::DisabledByPolicy;
    }

    void StartupTask::Enable(const char* taskName, bool enable)
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: taskName=%s, enable=%d", taskName, enable);

        EnableAsync(to_hstring(taskName), enable);
    }
}
