//
// SystemInformation.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include <pch.h>

#include "SystemInformation.h"
#include "SystemInformation.tmh"

#include "Environment.h"

namespace win32
{
    void SystemInformation::PrintAll()
    {
        PrintWindowsVersion();
        PrintSystemInfo();
        PrintProcessInfo();
        PrintUsername();
    }

    void SystemInformation::PrintWindowsVersion()
    {
        CRegKey key;
        LONG nError = key.Open(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", KEY_READ);
        if (nError == ERROR_SUCCESS)
        {
            TCHAR chBuffer[256]{};
            ULONG nChars = _countof(chBuffer);
            nError = key.QueryStringValue(L"LCUVer", chBuffer, &nChars);
            if (nError == ERROR_SUCCESS)
            {
                CString version(chBuffer, nChars);
                DoTraceMessage(WppVerbose, "Windows Version: %S", version);
            }
        }
    }

    void SystemInformation::PrintSystemInfo()
    {
        SYSTEM_INFO systemInfo = {};
        GetSystemInfo(&systemInfo);
        DoTraceMessage(WppVerbose, "SystemInfo: NumberOfProcessors: %u, ProcessorArchitecture: %u, ProcessorType: %u, ProcessorLevel: %u, ProcessorRevision: %u",
            systemInfo.dwNumberOfProcessors,
            systemInfo.wProcessorArchitecture,
            systemInfo.dwProcessorType,
            systemInfo.wProcessorLevel,
            systemInfo.wProcessorRevision);
    }

    void SystemInformation::PrintProcessInfo()
    {
        DoTraceMessage(WppVerbose, "ProcessInfo: Path='%!wstr!', IntegrityLevel=%s, AppContainer=%s",
            Environment::GetProcessPath().native(),
            MapProcessIntegrityLevel(Environment::GetProcessIntegrityLevel()),
            Environment::IsProcessAppContainer() ? "Yes" : "No");
    }

    void SystemInformation::PrintUsername()
    {
        DoTraceMessage(WppVerbose, "Username: %!str!", Environment::GetUsername());
    }

    const char* SystemInformation::MapProcessIntegrityLevel(DWORD dwIntegrityLevel)
    {
        if (dwIntegrityLevel >= SECURITY_MANDATORY_SYSTEM_RID)
        {
            return "System";
        }
        else if (dwIntegrityLevel >= SECURITY_MANDATORY_HIGH_RID)
        {
            return "High";
        }
        else if (dwIntegrityLevel >= SECURITY_MANDATORY_MEDIUM_RID)
        {
            return "Medium";
        }
        else if (dwIntegrityLevel >= SECURITY_MANDATORY_LOW_RID)
        {
            return "Low";
        }
        else if (dwIntegrityLevel > 0)
        {
            return "Untrusted";
        }
        else
        {
            return "Unknown";
        }
    }
}
