//
// UrlLauncher.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include "pch.h"

#include "UrlLauncher.h"
#include "UrlLauncher.tmh"

#include "Unicode.h"

namespace win32
{
    using namespace std::literals::string_view_literals;

    static bool IStartsWith(std::string_view text, std::string_view what)
    {
        return text.size() >= what.size() && _strnicmp(text.data(), what.data(), what.size()) == 0;
    }

    std::error_code UrlLauncher::Launch(std::string_view url)
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: url=%!sv!", url);

        if (!IStartsWith(url, "http://"sv) && !IStartsWith(url, "https://"sv))
        {
            return std::make_error_code(std::errc::invalid_argument);
        }

        std::wstring fileW = Unicode::FromUtf8(url);

        SHELLEXECUTEINFO info = { sizeof(info) };
        info.lpVerb = L"open";
        info.lpFile = fileW.c_str();
        info.nShow = SW_SHOWNORMAL;

        if (!::ShellExecuteExW(&info))
        {
            DWORD dwError = GetLastError();
            DoTraceMessage(WppError, "ShellExecuteExW failed: %!WINERROR!", dwError);
            return std::error_code(dwError, std::system_category());
        }

        return {};
    }
}
