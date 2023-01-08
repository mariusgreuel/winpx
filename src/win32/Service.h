//
// Service.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include <mutex>
#include <string>
#include <vector>

namespace win32
{
    class Service
    {
    public:
        Service();
        ~Service();

        Service(const Service&) = delete;
        Service& operator=(const Service&) = delete;

        HRESULT Dispatch();

    protected:
        static void WINAPI StaticMain(DWORD dwNumServicesArgs, LPWSTR* lpServiceArgVectors);
        void Main(DWORD dwNumServicesArgs, LPWSTR* lpServiceArgVectors);

        static DWORD WINAPI StaticControlHandler(DWORD dwControl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext);
        virtual DWORD ControlHandler(DWORD dwControl, DWORD dwEventType, LPVOID lpEventData);

        virtual void Start() = 0;
        virtual void Run() = 0;
        virtual void Stop() = 0;

        void CollectCommandLineArguments();
        void CollectServiceArguments(DWORD dwNumServicesArgs, LPWSTR* lpServiceArgVectors);

        void ReportStatus(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint);

        void ReportInformationMessage(WORD wCategory, DWORD dwEventID, const std::vector<std::wstring>& extra = {});
        void ReportWarningMessage(WORD wCategory, DWORD dwEventID, const std::vector<std::wstring>& extra = {});
        void ReportErrorMessage(WORD wCategory, DWORD dwEventID, const std::vector<std::wstring>& extra = {});
        void ReportMessage(WORD wType, WORD wCategory, DWORD dwEventID, const std::vector<std::wstring>& extra = {});

    protected:
        static Service* m_instance;
        static constexpr DWORD dwDefaultWaitHint = 1000;
        SERVICE_STATUS_HANDLE m_hService = nullptr;
        SERVICE_STATUS m_serviceStatus = {};
        std::mutex m_statusMutex;
        std::string m_serviceName;
        std::vector<std::string> m_arguments;
    };
}
