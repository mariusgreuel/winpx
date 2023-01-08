//
// RunAsAdmin.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include <pch.h>

#include "RunAsAdmin.h"
#include "RunAsAdmin.tmh"

#include "Unicode.h"

namespace win32
{
    std::error_code RunAsAdmin::Launch(const std::filesystem::path& exe, const std::string& arguments)
    {
        DoTraceMessage(WppVerbose, "%!FUNC!");

        std::wstring fileW = exe.native();
        std::wstring argumentsW = Unicode::FromUtf8(arguments);

        SHELLEXECUTEINFO info = { sizeof(info) };
        info.lpVerb = L"runas";
        info.lpFile = fileW.c_str();
        info.lpParameters = argumentsW.c_str();
        info.nShow = SW_SHOWNORMAL;
        info.fMask = SEE_MASK_NOCLOSEPROCESS;

        if (!ShellExecuteExW(&info))
        {
            DWORD dwError = GetLastError();
            DoTraceMessage(WppError, "ShellExecuteExW failed: %!WINERROR!", dwError);
            return std::error_code(dwError, std::system_category());
        }

        if (info.hProcess != nullptr)
        {
            WaitForSingleObject(info.hProcess, INFINITE);
            CloseHandle(info.hProcess);
        }

        return {};
    }
}
