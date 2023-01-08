//
// Unicode.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include <pch.h>

#include "Unicode.h"
#include "Unicode.tmh"

#include "Win32Error.h"

namespace win32
{
    std::wstring Unicode::MultiByteToWideChar(const char* text, uint32_t codePage)
    {
        return MultiByteToWideChar(text, std::strlen(text), codePage);
    }

    std::wstring Unicode::MultiByteToWideChar(const char* text, size_t chars, uint32_t codePage)
    {
        if (chars == 0)
            return {};

        int nChars = CastToInt(chars);
        int nCharsRequired = ::MultiByteToWideChar(codePage, 0, text, nChars, nullptr, 0);
        if (nCharsRequired == 0)
        {
            DWORD dwError = GetLastError();
            DoTraceMessage(WppError, "MultiByteToWideChar failed: %!WINERROR!", dwError);
            throw Win32Error(dwError, "MultiByteToWideChar failed.");
        }

        std::wstring str(nCharsRequired, L'\0');
        nCharsRequired = ::MultiByteToWideChar(codePage, 0, text, nChars, str.data(), static_cast<int>(str.size()));
        if (nCharsRequired == 0)
        {
            DWORD dwError = GetLastError();
            DoTraceMessage(WppError, "MultiByteToWideChar failed: %!WINERROR!", dwError);
            throw Win32Error(dwError, "MultiByteToWideChar failed.");
        }

        return str;
    }

    std::wstring Unicode::MultiByteToWideChar(std::string_view text, uint32_t codePage)
    {
        return MultiByteToWideChar(text.data(), text.size(), codePage);
    }

    std::string Unicode::WideCharToMultiByte(const wchar_t* text, uint32_t codePage)
    {
        return WideCharToMultiByte(text, std::wcslen(text), codePage);
    }

    std::string Unicode::WideCharToMultiByte(const wchar_t* text, size_t chars, uint32_t codePage)
    {
        if (chars == 0)
            return {};

        int nChars = CastToInt(chars);
        int nCharsRequired = ::WideCharToMultiByte(codePage, 0, text, nChars, nullptr, 0, nullptr, nullptr);
        if (nCharsRequired == 0)
        {
            DWORD dwError = GetLastError();
            DoTraceMessage(WppError, "WideCharToMultiByte failed: %!WINERROR!", dwError);
            throw Win32Error(dwError, "WideCharToMultiByte failed.");
        }

        std::string str(nCharsRequired, '\0');
        nCharsRequired = ::WideCharToMultiByte(codePage, 0, text, nChars, str.data(), static_cast<int>(str.size()), nullptr, nullptr);
        if (nCharsRequired == 0)
        {
            DWORD dwError = GetLastError();
            DoTraceMessage(WppError, "WideCharToMultiByte failed: %!WINERROR!", dwError);
            throw Win32Error(dwError, "WideCharToMultiByte failed.");
        }

        return str;
    }

    std::string Unicode::WideCharToMultiByte(std::wstring_view text, uint32_t codePage)
    {
        return WideCharToMultiByte(text.data(), text.size(), codePage);
    }

    std::string Unicode::ToUtf8(std::wstring_view text)
    {
        return Unicode::WideCharToMultiByte(text, CP_UTF8);
    }

    std::wstring Unicode::FromUtf8(std::string_view text)
    {
        return Unicode::MultiByteToWideChar(text, CP_UTF8);
    }

    int Unicode::CastToInt(size_t count)
    {
        if (count > static_cast<size_t>(std::numeric_limits<int>::max()))
            throw std::length_error("Unicode input exceeds int maximum.");

        return static_cast<int>(count);
    }
}
