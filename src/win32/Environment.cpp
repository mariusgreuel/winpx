//
// Environment.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include <pch.h>

#include "Environment.h"
#include "Environment.tmh"

#include "Unicode.h"
#include "Win32.h"

namespace win32
{
    using namespace std::literals::string_view_literals;

    std::string Environment::GetUsername()
    {
        WCHAR acUsername[256] = {};
        DWORD dwChars = _countof(acUsername);
        if (!GetUserNameW(acUsername, &dwChars) || dwChars <= 1)
        {
            return {};
        }

        return Unicode::ToUtf8(std::wstring(acUsername, dwChars - 1));
    }

    std::filesystem::path Environment::GetProcessPath()
    {
        return Win32::GetModulePath(nullptr);
    }

    bool Environment::IsDarkMode()
    {
        bool isDarkMode = false;

        CRegKey key;
        if (key.Open(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", KEY_READ) == ERROR_SUCCESS)
        {
            DWORD dwValue = 0;
            if (key.QueryDWORDValue(L"SystemUsesLightTheme", dwValue) == ERROR_SUCCESS)
            {
                isDarkMode = dwValue == 0;
            }
        }

        return isDarkMode;
    }

    bool Environment::IsRemoteDesktopSession()
    {
        return GetSystemMetrics(SM_REMOTESESSION) != 0;
    }

    bool Environment::IsLocalAdministrator()
    {
        BOOL bIsAdmin = FALSE;

        PSID pSid = nullptr;
        SID_IDENTIFIER_AUTHORITY authority = SECURITY_NT_AUTHORITY;
        if (::AllocateAndInitializeSid(&authority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &pSid))
        {
            if (!::CheckTokenMembership(nullptr, pSid, &bIsAdmin))
            {
                bIsAdmin = FALSE;
            }

            ::FreeSid(pSid);
        }

        return bIsAdmin != FALSE;
    }

    bool Environment::IsProcessAppContainer()
    {
        DWORD dwIsAppContainer = 0;

        HANDLE hToken = nullptr;
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
        {
            DWORD dwLengthNeeded = sizeof(dwIsAppContainer);
            GetTokenInformation(hToken, TokenIsAppContainer, &dwIsAppContainer, dwLengthNeeded, &dwLengthNeeded);

            CloseHandle(hToken);
        }

        return dwIsAppContainer != 0;
    }

    DWORD Environment::GetProcessIntegrityLevel()
    {
        DWORD dwIntegrityLevel = 0;

        HANDLE hToken = nullptr;
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
        {
            DWORD dwLengthNeeded = 0;
            if (!GetTokenInformation(hToken, TokenIntegrityLevel, nullptr, 0, &dwLengthNeeded))
            {
                DWORD dwError = GetLastError();
                if (dwError != ERROR_INSUFFICIENT_BUFFER)
                {
                    CloseHandle(hToken);
                    return 0;
                }
            }

            CHeapPtr<TOKEN_MANDATORY_LABEL> pTokenIntegrityLevel;
            if (pTokenIntegrityLevel.AllocateBytes(dwLengthNeeded))
            {
                if (GetTokenInformation(hToken, TokenIntegrityLevel, pTokenIntegrityLevel, dwLengthNeeded, &dwLengthNeeded))
                {
                    UCHAR subAuthorityCount = *GetSidSubAuthorityCount(pTokenIntegrityLevel->Label.Sid);
                    dwIntegrityLevel = *GetSidSubAuthority(pTokenIntegrityLevel->Label.Sid, subAuthorityCount - 1);
                }
            }

            CloseHandle(hToken);
        }

        return dwIntegrityLevel;
    }

    HRESULT Environment::SetAutoStartApp(std::string_view keyName, std::string_view value, bool enabled)
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: keyName='%!sv!', value='%!sv!'", keyName, value);

        CRegKey key;
        LSTATUS nError = key.Open(HKEY_CURRENT_USER, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Run"), KEY_READ | KEY_WRITE);
        if (nError != ERROR_SUCCESS)
        {
            DoTraceMessage(WppError, "CRegKey::Open failed: %!WINERROR!", nError);
            return AtlHresultFromWin32(nError);
        }

        if (enabled)
        {
            nError = key.SetStringValue(Unicode::FromUtf8(keyName).c_str(), Unicode::FromUtf8(value).c_str());
            if (nError != ERROR_SUCCESS)
            {
                DoTraceMessage(WppError, "CRegKey::SetStringValue failed: %!WINERROR!", nError);
                return AtlHresultFromWin32(nError);
            }
        }
        else
        {
            nError = key.DeleteValue(Unicode::FromUtf8(keyName).c_str());
            if (nError == ERROR_FILE_NOT_FOUND)
                return S_FALSE;

            if (nError != ERROR_SUCCESS)
            {
                DoTraceMessage(WppError, "CRegKey::DeleteValue failed: %!WINERROR!", nError);
                return AtlHresultFromWin32(nError);
            }
        }

        return S_OK;
    }

    HRESULT Environment::SetUserVariable(std::string_view name, std::string_view value)
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: name='%s', value='%s'", name.data(), value.data());

        return SetOrRemoveUserVariable(name, value);
    }

    HRESULT Environment::RemoveUserVariable(std::string_view name)
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: pszKeyName='%!sv!'", name);

        return SetOrRemoveUserVariable(name, ""sv);
    }

    HRESULT Environment::SetOrRemoveUserVariable(std::string_view name, std::string_view value)
    {
        CRegKey key;
        LSTATUS nError = key.Open(HKEY_CURRENT_USER, _T("Environment"), KEY_READ | KEY_WRITE);
        if (nError != ERROR_SUCCESS)
        {
            DoTraceMessage(WppError, "CRegKey::Open failed: %!WINERROR!", nError);
            return AtlHresultFromWin32(nError);
        }

        auto nameW = Unicode::FromUtf8(name);
        auto valueW = Unicode::FromUtf8(value);

        if (!value.empty())
        {
            ULONG nChars = 0;
            nError = key.QueryStringValue(nameW.c_str(), nullptr, &nChars);
            if (nError == ERROR_SUCCESS)
            {
                CString currentValue;
                nError = key.QueryStringValue(nameW.c_str(), currentValue.GetBuffer(nChars), &nChars);
                currentValue.ReleaseBuffer();

                if (nError == ERROR_SUCCESS && currentValue == valueW.c_str())
                {
                    return S_FALSE;
                }
            }

            nError = key.SetStringValue(nameW.c_str(), valueW.c_str());
            if (nError != ERROR_SUCCESS)
            {
                DoTraceMessage(WppError, "CRegKey::SetStringValue failed: %!WINERROR!", nError);
                return AtlHresultFromWin32(nError);
            }
        }
        else
        {
            nError = key.DeleteValue(nameW.c_str());
            if (nError == ERROR_FILE_NOT_FOUND)
                return S_FALSE;

            if (nError != ERROR_SUCCESS)
            {
                DoTraceMessage(WppError, "CRegKey::DeleteValue failed: %!WINERROR!", nError);
                return AtlHresultFromWin32(nError);
            }
        }

        return S_OK;
    }

    HRESULT Environment::SendSettingChangeNotification(std::string_view area)
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: pszArea='%!sv!'", area);

        DWORD_PTR result = 0;
        if (!SendMessageTimeoutW(
                HWND_BROADCAST,
                WM_SETTINGCHANGE,
                0,
                reinterpret_cast<LPARAM>(Unicode::FromUtf8(area).c_str()),
                SMTO_ABORTIFHUNG,
                5000,
                &result))
        {
            DWORD dwError = GetLastError();
            DoTraceMessage(WppError, "SendMessageTimeoutW failed: %!WINERROR!", dwError);
            return AtlHresultFromWin32(dwError);
        }

        return S_OK;
    }
}
