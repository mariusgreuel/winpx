//
// ServiceInstaller.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include <filesystem>
#include <string>

namespace win32
{
    class ServiceInstaller
    {
    public:
        ServiceInstaller();
        ~ServiceInstaller();

        ServiceInstaller(const ServiceInstaller&) = delete;
        ServiceInstaller& operator=(const ServiceInstaller&) = delete;

        HRESULT Install();
        HRESULT Uninstall();
        HRESULT Start();
        HRESULT Stop();
        HRESULT SetEventLogMessageFile(DWORD dwTypesSupported = 7);

        std::string serviceName;
        std::string displayName;
        std::string description;
        std::filesystem::path binaryPath;
        std::string serviceArguments;
        std::string dependencies;
        std::string startName;
        std::string password;
        DWORD dwDesiredAccess = SERVICE_ALL_ACCESS;
        DWORD dwServiceType = SERVICE_WIN32_OWN_PROCESS;
        DWORD dwStartType = SERVICE_AUTO_START;
        DWORD dwErrorControl = SERVICE_ERROR_NORMAL;

    private:
        HRESULT OpenServiceManager();
        void CloseHandles();
        bool WaitForServiceToStop(SC_HANDLE hService);

    private:
        SC_HANDLE m_hDatabase = nullptr;
        SC_HANDLE m_hService = nullptr;
    };
}
