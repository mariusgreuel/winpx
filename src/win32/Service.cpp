//
// Service.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include <pch.h>

#include "Service.h"
#include "Service.tmh"

#include "Unicode.h"
#include "Win32Error.h"

namespace win32
{
    enum class ServiceStatusCode
    {
        STOPPED = SERVICE_STOPPED,
        START_PENDING = SERVICE_START_PENDING,
        STOP_PENDING = SERVICE_STOP_PENDING,
        RUNNING = SERVICE_RUNNING,
        CONTINUE_PENDING = SERVICE_CONTINUE_PENDING,
        PAUSE_PENDING = SERVICE_PAUSE_PENDING,
        PAUSED = SERVICE_PAUSED,
    };

    enum class ServiceControlCode
    {
        STOP = SERVICE_CONTROL_STOP,
        PAUSE = SERVICE_CONTROL_PAUSE,
        CONTINUE = SERVICE_CONTROL_CONTINUE,
        INTERROGATE = SERVICE_CONTROL_INTERROGATE,
        SHUTDOWN = SERVICE_CONTROL_SHUTDOWN,
        PARAMCHANGE = SERVICE_CONTROL_PARAMCHANGE,
    };

    Service::Service()
    {
        DoTraceMessage(WppObject, "%!FUNC!");

        m_instance = this;
    }

    Service::~Service()
    {
        DoTraceMessage(WppObject, "%!FUNC!");

        m_instance = nullptr;
    }

    HRESULT Service::Dispatch()
    {
        DoTraceMessage(WppService, "%!FUNC!");

        auto serviceNameW = Unicode::FromUtf8(m_serviceName);

        const SERVICE_TABLE_ENTRYW dispatchTable[] = {
            { serviceNameW.data(), StaticMain },
            { nullptr, nullptr }
        };

        if (!StartServiceCtrlDispatcherW(dispatchTable))
        {
            DWORD dwError = GetLastError();
            if (dwError == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT)
            {
                DoTraceMessage(WppVerbose, "Not running as a service.");
                return S_FALSE;
            }
            else
            {
                DoTraceMessage(WppError, "StartServiceCtrlDispatcherW failed: %!WINERROR!", dwError);
                return HRESULT_FROM_WIN32(dwError);
            }
        }

        return S_OK;
    }

    void Service::StaticMain(DWORD dwNumServicesArgs, LPWSTR* lpServiceArgVectors)
    {
        m_instance->Main(dwNumServicesArgs, lpServiceArgVectors);
    }

    void Service::Main(DWORD dwNumServicesArgs, LPWSTR* lpServiceArgVectors)
    {
        if (lpServiceArgVectors != nullptr)
        {
            m_serviceName = Unicode::ToUtf8(lpServiceArgVectors[0]);
        }

        DoTraceMessage(WppService, "%!FUNC!: %!str!", m_serviceName);

        try
        {
            auto hService = RegisterServiceCtrlHandlerExW(Unicode::FromUtf8(m_serviceName).c_str(), StaticControlHandler, this);
            if (!hService)
            {
                DWORD dwError = GetLastError();
                DoTraceMessage(WppError, "RegisterServiceCtrlHandlerExW failed: %!WINERROR!", dwError);
                throw Win32Error(dwError, "RegisterServiceCtrlHandlerExW failed");
            }

            CollectCommandLineArguments();
            CollectServiceArguments(dwNumServicesArgs, lpServiceArgVectors);

            m_hService = hService;
            m_serviceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;

            ReportStatus(SERVICE_START_PENDING, NO_ERROR, dwDefaultWaitHint);
            Start();
            ReportStatus(SERVICE_RUNNING, NO_ERROR, 0);
            Run();
            ReportStatus(SERVICE_STOPPED, NO_ERROR, 0);
        }
        catch (const std::system_error& e)
        {
            DoTraceMessage(WppError, "Service failed to start: %s", e.what());
            ReportStatus(SERVICE_STOPPED, e.code().value(), 0);
        }
        catch (const std::exception& e)
        {
            DoTraceMessage(WppError, "Service failed to start: %s", e.what());
            ReportStatus(SERVICE_STOPPED, ERROR_INTERNAL_ERROR, 0);
        }
    }

    DWORD Service::StaticControlHandler(DWORD dwControl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext)
    {
        return static_cast<Service*>(lpContext)->ControlHandler(dwControl, dwEventType, lpEventData);
    }

    DWORD Service::ControlHandler(DWORD dwControl, DWORD dwEventType, LPVOID /* lpEventData */)
    {
        DoTraceMessage(WppService, "%!FUNC!: dwControl=%!SERVICE_CONTROL_CODE!, dwEventType=%u", dwControl, dwEventType);

        try
        {
            if (dwControl == SERVICE_CONTROL_STOP)
            {
                ReportStatus(SERVICE_STOP_PENDING, NO_ERROR, dwDefaultWaitHint);
                Stop();
                return NO_ERROR;
            }
            else if (dwControl == SERVICE_CONTROL_INTERROGATE)
            {
                return NO_ERROR;
            }
            else
            {
                return ERROR_CALL_NOT_IMPLEMENTED;
            }
        }
        catch (const std::system_error& e)
        {
            DoTraceMessage(WppError, "Service failed to stop: %s", e.what());
            ReportStatus(SERVICE_STOPPED, e.code().value(), 0);
            return e.code().value();
        }
        catch (const std::exception& e)
        {
            DoTraceMessage(WppError, "Service failed to stop: %s", e.what());
            ReportStatus(SERVICE_STOPPED, ERROR_INTERNAL_ERROR, 0);
            return ERROR_INTERNAL_ERROR;
        }
    }

    void Service::CollectCommandLineArguments()
    {
        int nArguments = 0;
        CHeapPtr<LPWSTR, CLocalAllocator> pszArguments(CommandLineToArgvW(GetCommandLineW(), &nArguments));
        if (pszArguments != nullptr)
        {
            for (int i = 1; i < nArguments; i++)
            {
                m_arguments.emplace_back(Unicode::ToUtf8(pszArguments[i]));
            }
        }
    }

    void Service::CollectServiceArguments(DWORD dwNumServicesArgs, LPWSTR* lpServiceArgVectors)
    {
        if (lpServiceArgVectors != nullptr)
        {
            for (DWORD i = 1; i < dwNumServicesArgs; ++i)
            {
                m_arguments.emplace_back(Unicode::ToUtf8(lpServiceArgVectors[i]));
            }
        }
    }

    void Service::ReportStatus(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint)
    {
        DoTraceMessage(WppService, "%!FUNC!: dwCurrentState=%!SERVICE_STATUS_CODE!, dwWin32ExitCode=%!WINERROR!, dwWaitHint=%u", dwCurrentState, dwWin32ExitCode, dwWaitHint);

        std::scoped_lock lock(m_statusMutex);

        if (dwCurrentState == SERVICE_RUNNING || dwCurrentState == SERVICE_STOPPED)
        {
            m_serviceStatus.dwCheckPoint = 0;
        }
        else
        {
            m_serviceStatus.dwCheckPoint++;
        }

        m_serviceStatus.dwCurrentState = dwCurrentState;
        m_serviceStatus.dwControlsAccepted = dwCurrentState == SERVICE_START_PENDING ? 0 : SERVICE_ACCEPT_STOP;
        m_serviceStatus.dwWin32ExitCode = dwWin32ExitCode;
        m_serviceStatus.dwWaitHint = dwWaitHint;

        if (!SetServiceStatus(m_hService, &m_serviceStatus))
        {
            DWORD dwError = GetLastError();
            DoTraceMessage(WppError, "SetServiceStatus failed: %!WINERROR!", dwError);
        }
    }

    void Service::ReportInformationMessage(WORD wCategory, DWORD dwEventID, const std::vector<std::wstring>& extra)
    {
        ReportMessage(EVENTLOG_INFORMATION_TYPE, wCategory, dwEventID, extra);
    }

    void Service::ReportWarningMessage(WORD wCategory, DWORD dwEventID, const std::vector<std::wstring>& extra)
    {
        ReportMessage(EVENTLOG_WARNING_TYPE, wCategory, dwEventID, extra);
    }

    void Service::ReportErrorMessage(WORD wCategory, DWORD dwEventID, const std::vector<std::wstring>& extra)
    {
        ReportMessage(EVENTLOG_ERROR_TYPE, wCategory, dwEventID, extra);
    }

    void Service::ReportMessage(WORD wType, WORD wCategory, DWORD dwEventID, const std::vector<std::wstring>& extra)
    {
        DoTraceMessage(WppService, "%!FUNC!: wType=%u, wCategory=%u, dwEventID=%u", wType, wCategory, dwEventID);

        auto serviceNameW = Unicode::FromUtf8(m_serviceName);

        HANDLE hEventSource = RegisterEventSourceW(nullptr, serviceNameW.c_str());
        if (hEventSource == nullptr)
        {
            DWORD dwError = GetLastError();
            DoTraceMessage(WppError, "RegisterEventSourceW failed: %!WINERROR!", dwError);
            return;
        }

        std::vector<LPCWSTR> strings;
        strings.push_back(serviceNameW.c_str());
        for (const auto& str : extra)
        {
            strings.push_back(str.c_str());
        }

        if (!ReportEventW(hEventSource, wType, wCategory, dwEventID, nullptr, static_cast<WORD>(strings.size()), 0, strings.data(), nullptr))
        {
            DWORD dwError = GetLastError();
            DoTraceMessage(WppError, "ReportEventW failed: %!WINERROR!", dwError);
        }

        DeregisterEventSource(hEventSource);
    }

    Service* Service::m_instance = nullptr;

    INCLUDE_IN_PDB(ServiceStatusCode);
    INCLUDE_IN_PDB(ServiceControlCode);
}
