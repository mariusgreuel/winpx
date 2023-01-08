//
// ServiceInstaller.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include <pch.h>

#include "ServiceInstaller.h"
#include "ServiceInstaller.tmh"

#include "Environment.h"
#include "Unicode.h"

namespace win32
{
    ServiceInstaller::ServiceInstaller() : binaryPath(Environment::GetProcessPath())
    {
        DoTraceMessage(WppObject, "%!FUNC!");
    }

    ServiceInstaller::~ServiceInstaller()
    {
        DoTraceMessage(WppObject, "%!FUNC!");

        CloseHandles();
    }

    HRESULT ServiceInstaller::Install()
    {
        DoTraceMessage(WppService, "%!FUNC!");

        HRESULT hr = OpenServiceManager();
        if (FAILED(hr))
        {
            return hr;
        }

        std::string commandLine = std::format("\"{}\"", Unicode::ToUtf8(binaryPath.native()));
        if (!serviceArguments.empty())
        {
            if (serviceArguments.front() != ' ')
                commandLine += ' ';

            commandLine += serviceArguments;
        }

        SC_HANDLE hService = CreateServiceW(
            m_hDatabase,
            Unicode::FromUtf8(serviceName).c_str(),
            Unicode::FromUtf8(displayName).c_str(),
            dwDesiredAccess,
            dwServiceType,
            dwStartType,
            dwErrorControl,
            Unicode::FromUtf8(commandLine).c_str(),
            nullptr,
            nullptr,
            dependencies.empty() ? nullptr : Unicode::FromUtf8(dependencies).c_str(),
            startName.empty() ? nullptr : Unicode::FromUtf8(startName).c_str(),
            password.empty() ? nullptr : Unicode::FromUtf8(password).c_str());
        if (!hService)
        {
            DWORD dwError = GetLastError();
            DoTraceMessage(WppError, "CreateServiceW failed: %!WINERROR!", dwError);
            return HRESULT_FROM_WIN32(dwError);
        }

        m_hService = hService;

        if (!description.empty())
        {
            auto descriptionw = Unicode::FromUtf8(description);
            SERVICE_DESCRIPTIONW info = {};
            info.lpDescription = descriptionw.data();
            if (!ChangeServiceConfig2W(hService, SERVICE_CONFIG_DESCRIPTION, &info))
            {
                DWORD dwError = GetLastError();
                DoTraceMessage(WppWarning, "ChangeServiceConfig2W failed: %!WINERROR!", dwError);
            }
        }

        return S_OK;
    }

    HRESULT ServiceInstaller::Uninstall()
    {
        DoTraceMessage(WppService, "%!FUNC!");

        HRESULT hr = OpenServiceManager();
        if (FAILED(hr))
        {
            return hr;
        }

        SC_HANDLE hService = OpenServiceW(m_hDatabase, Unicode::FromUtf8(serviceName).c_str(), DELETE | SERVICE_STOP | SERVICE_QUERY_STATUS);
        if (!hService)
        {
            DWORD dwError = GetLastError();
            DoTraceMessage(WppError, "OpenServiceW failed: %!WINERROR!", dwError);
            return HRESULT_FROM_WIN32(dwError);
        }

        m_hService = hService;

        Stop();

        if (!DeleteService(hService))
        {
            DWORD dwError = GetLastError();
            DoTraceMessage(WppError, "DeleteService failed: %!WINERROR!", dwError);
            return HRESULT_FROM_WIN32(dwError);
        }

        return S_OK;
    }

    HRESULT ServiceInstaller::Start()
    {
        DoTraceMessage(WppService, "%!FUNC!");

        if (!StartServiceW(m_hService, 0, nullptr))
        {
            DWORD dwError = GetLastError();
            if (dwError != ERROR_SERVICE_ALREADY_RUNNING)
            {
                DoTraceMessage(WppWarning, "StartService failed: %!WINERROR!", dwError);
                return HRESULT_FROM_WIN32(dwError);
            }
        }

        return S_OK;
    }

    HRESULT ServiceInstaller::Stop()
    {
        DoTraceMessage(WppService, "%!FUNC!");

        SERVICE_STATUS serviceStatus = {};
        if (!ControlService(m_hService, SERVICE_CONTROL_STOP, &serviceStatus))
        {
            DWORD dwError = GetLastError();
            if (dwError != ERROR_SERVICE_NOT_ACTIVE)
            {
                DoTraceMessage(WppWarning, "ControlService failed: %!WINERROR!", dwError);
                return HRESULT_FROM_WIN32(dwError);
            }
        }
        else if (!WaitForServiceToStop(m_hService))
        {
            DWORD dwError = GetLastError();
            DoTraceMessage(WppWarning, "Service did not stop: %!WINERROR!", dwError);
            return HRESULT_FROM_WIN32(dwError);
        }

        return S_OK;
    }

    HRESULT ServiceInstaller::SetEventLogMessageFile(DWORD dwTypesSupported)
    {
        DoTraceMessage(WppService, "%!FUNC!");

        CRegKey keyApplication;
        LONG nError = keyApplication.Open(HKEY_LOCAL_MACHINE, _T("SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application"), KEY_WRITE);
        if (nError != ERROR_SUCCESS)
        {
            HRESULT hr = HRESULT_FROM_WIN32(nError);
            DoTraceMessage(WppError, "Failed to open registry key: %!HRESULT!", hr);
            return hr;
        }

        CRegKey keyWinPx;
        nError = keyWinPx.Create(keyApplication, Unicode::FromUtf8(serviceName).c_str());
        if (nError != ERROR_SUCCESS)
        {
            HRESULT hr = HRESULT_FROM_WIN32(nError);
            DoTraceMessage(WppError, "Failed to create registry key: %!HRESULT!", hr);
            return hr;
        }

        nError = keyWinPx.SetStringValue(_T("EventMessageFile"), binaryPath.native().c_str());
        if (nError != ERROR_SUCCESS)
        {
            HRESULT hr = HRESULT_FROM_WIN32(nError);
            DoTraceMessage(WppError, "Failed to set 'EventMessageFile' value: %!HRESULT!", hr);
            return hr;
        }

        nError = keyWinPx.SetDWORDValue(_T("TypesSupported"), dwTypesSupported);
        if (nError != ERROR_SUCCESS)
        {
            HRESULT hr = HRESULT_FROM_WIN32(nError);
            DoTraceMessage(WppError, "Failed to set 'TypesSupported' value: %!HRESULT!", hr);
            return hr;
        }

        return S_OK;
    }

    HRESULT ServiceInstaller::OpenServiceManager()
    {
        if (m_hDatabase != nullptr)
            return S_FALSE;

        SC_HANDLE hDatabase = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
        if (!hDatabase)
        {
            DWORD dwError = GetLastError();
            DoTraceMessage(WppError, "OpenSCManagerW failed: %!WINERROR!", dwError);
            return HRESULT_FROM_WIN32(dwError);
        }

        m_hDatabase = hDatabase;

        return S_OK;
    }

    void ServiceInstaller::CloseHandles()
    {
        if (m_hService != nullptr)
        {
            CloseServiceHandle(m_hService);
            m_hService = nullptr;
        }

        if (m_hDatabase != nullptr)
        {
            CloseServiceHandle(m_hDatabase);
            m_hDatabase = nullptr;
        }
    }

    bool ServiceInstaller::WaitForServiceToStop(SC_HANDLE hService)
    {
        while (true)
        {
            SERVICE_STATUS_PROCESS status{};
            DWORD bytesNeeded = 0;
            if (!QueryServiceStatusEx(hService, SC_STATUS_PROCESS_INFO, reinterpret_cast<LPBYTE>(&status), sizeof(status), &bytesNeeded))
            {
                return false;
            }

            if (status.dwCurrentState == SERVICE_STOPPED)
            {
                return true;
            }
            else if (status.dwCurrentState != SERVICE_STOP_PENDING)
            {
                SetLastError(ERROR_SERVICE_CANNOT_ACCEPT_CTRL);
                return false;
            }
            else
            {
                DWORD dwWaitTime = status.dwWaitHint < 10000 ? status.dwWaitHint / 10 : 1000;
                DoTraceMessage(WppVerbose, "%!FUNC! Waiting for %u milliseconds", dwWaitTime);
                Sleep(dwWaitTime);
            }
        }
    }
}
