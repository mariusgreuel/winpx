//
// Win32.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include <pch.h>

#include "Win32.h"
#include "Win32.tmh"

#include "Unicode.h"

namespace win32
{
    std::string Win32::FormatErrorCode(uint32_t error)
    {
        if (error == ERROR_SUCCESS)
        {
            return "Success";
        }

        CHeapPtr<WCHAR, CLocalAllocator> pMessage;
        DWORD dwFlags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
        HMODULE hModule = GetModuleHandleW(L"winhttp.dll");
        DWORD dwChars = FormatMessageW(dwFlags, hModule, error, 0, reinterpret_cast<LPWSTR>(&pMessage.m_pData), 0, nullptr);
        if (dwChars == 0)
        {
            return std::format("System error {}.", error);
        }

        if (dwChars > 0 && pMessage[dwChars - 1] == L'\n')
        {
            --dwChars;
        }

        if (dwChars > 0 && pMessage[dwChars - 1] == L'\r')
        {
            --dwChars;
        }

        return Unicode::ToUtf8(std::wstring_view(pMessage.m_pData, dwChars));
    }

    std::string Win32::FormatErrorCode(std::error_code code)
    {
        if (code.category() == std::system_category() || code.category() == asio::system_category())
        {
            return FormatErrorCode(code.value());
        }
        else
        {
            return code.message();
        }
    }

    std::filesystem::path Win32::GetKnownFolderPath(const KNOWNFOLDERID& rfid)
    {
        CComHeapPtr<WCHAR> pszPath;
        HRESULT hr = SHGetKnownFolderPath(rfid, 0, nullptr, &pszPath.m_pData);
        if (FAILED(hr))
        {
            DoTraceMessage(WppWarning, "SHGetKnownFolderPath failed: %!HRESULT!", hr);
            return {};
        }

        return pszPath.m_pData;
    }

    std::filesystem::path Win32::GetModulePath(HMODULE hModule)
    {
        std::vector<WCHAR> path(MAX_PATH);

        while (true)
        {
            DWORD dwChars = GetModuleFileNameW(hModule, path.data(), static_cast<DWORD>(path.size()));
            if (dwChars == 0)
            {
                DWORD dwError = GetLastError();
                DoTraceMessage(WppWarning, "GetModuleFileNameW failed: %!WINERROR!", dwError);
                return {};
            }

            if (dwChars >= path.size() - 1)
            {
                path.resize(static_cast<size_t>(dwChars) + 1);
                continue;
            }

            return std::wstring(path.data(), dwChars);
        }
    }
}
