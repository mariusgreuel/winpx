//
// Base64.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include <pch.h>

#include "Base64.h"
#include "Base64.tmh"

#include "Win32Error.h"

namespace win32
{
    static uint32_t CastToUInt32(size_t count)
    {
        if (count > std::numeric_limits<uint32_t>::max())
            throw std::length_error("Base64 input exceeds uint32_t maximum.");

        return static_cast<uint32_t>(count);
    }

    std::string Base64::Encode(std::string_view buffer)
    {
        return Encode(reinterpret_cast<const std::byte*>(buffer.data()), buffer.size());
    }

    std::string Base64::Encode(const std::vector<std::byte>& buffer)
    {
        return Encode(buffer.data(), buffer.size());
    }

    std::vector<std::byte> Base64::Decode(std::string_view buffer)
    {
        return Decode(buffer.data(), buffer.size());
    }

    std::string Base64::Encode(const std::byte* buffer, size_t count)
    {
        if (count == 0)
            return {};

        const DWORD dwFlags = CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF;

        DWORD dwChars = 0;
        if (!CryptBinaryToStringA(reinterpret_cast<const BYTE*>(buffer), CastToUInt32(count), dwFlags, nullptr, &dwChars))
        {
            DWORD dwError = GetLastError();
            DoTraceMessage(WppError, "CryptBinaryToStringA failed: %!WINERROR!", dwError);
            throw Win32Error(dwError, "CryptBinaryToStringA failed.");
        }

        std::string result(dwChars, '\0');
        if (!CryptBinaryToStringA(reinterpret_cast<const BYTE*>(buffer), CastToUInt32(count), dwFlags, result.data(), &dwChars))
        {
            DWORD dwError = GetLastError();
            DoTraceMessage(WppError, "CryptBinaryToStringA failed: %!WINERROR!", dwError);
            throw Win32Error(dwError, "CryptBinaryToStringA failed.");
        }

        result.resize(dwChars);
        return result;
    }

    std::vector<std::byte> Base64::Decode(const char* buffer, size_t count)
    {
        if (count == 0)
            return {};

        const DWORD dwFlags = CRYPT_STRING_BASE64;

        DWORD dwBytes = 0;
        if (!CryptStringToBinaryA(buffer, CastToUInt32(count), dwFlags, nullptr, &dwBytes, nullptr, nullptr))
        {
            DWORD dwError = GetLastError();
            DoTraceMessage(WppError, "CryptStringToBinaryA failed: %!WINERROR!", dwError);
            throw Win32Error(dwError, "CryptStringToBinaryA failed.");
        }

        if (dwBytes == 0)
            return {};

        std::vector<std::byte> result(dwBytes);
        if (!CryptStringToBinaryA(buffer, CastToUInt32(count), dwFlags, reinterpret_cast<BYTE*>(result.data()), &dwBytes, nullptr, nullptr))
        {
            DWORD dwError = GetLastError();
            DoTraceMessage(WppError, "CryptStringToBinaryA failed: %!WINERROR!", dwError);
            throw Win32Error(dwError, "CryptStringToBinaryA failed.");
        }

        result.resize(dwBytes);
        return result;
    }
}
