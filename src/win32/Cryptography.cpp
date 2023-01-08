//
// Cryptography.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include <pch.h>

#include "Cryptography.h"
#include "Cryptography.tmh"

#include "Base64.h"
#include "Win32Error.h"

namespace win32
{
    std::string Cryptography::ProtectData(const std::string& plaintext)
    {
        return Base64::Encode(ProtectData(ToBytes(plaintext)));
    }

    std::string Cryptography::UnprotectData(const std::string& ciphertext)
    {
        return ToString(UnprotectData(Base64::Decode(ciphertext)));
    }

    std::vector<std::byte> Cryptography::ProtectData(const std::vector<std::byte>& plaintext)
    {
        if (plaintext.empty())
            return {};

        DATA_BLOB in{};
        in.pbData = const_cast<BYTE*>(reinterpret_cast<const BYTE*>(plaintext.data()));
        in.cbData = CastToUInt32(plaintext.size());

        DATA_BLOB out{};
        if (!CryptProtectData(
                &in,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                CRYPTPROTECT_UI_FORBIDDEN,
                &out))
        {
            DWORD dwError = GetLastError();
            DoTraceMessage(WppError, "CryptProtectData failed: %!WINERROR!", dwError);
            throw Win32Error(dwError, "CryptProtectData failed.");
        }

        CHeapPtr<BYTE, CLocalAllocator> pbData(out.pbData);
        auto data = reinterpret_cast<const std::byte*>(pbData.m_pData);
        std::vector<std::byte> ciphertext(data, data + out.cbData);
        return ciphertext;
    }

    std::vector<std::byte> Cryptography::UnprotectData(const std::vector<std::byte>& ciphertext)
    {
        if (ciphertext.empty())
            return {};

        DATA_BLOB in{};
        in.pbData = const_cast<BYTE*>(reinterpret_cast<const BYTE*>(ciphertext.data()));
        in.cbData = CastToUInt32(ciphertext.size());

        DATA_BLOB out{};
        if (!CryptUnprotectData(
                &in,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                CRYPTPROTECT_UI_FORBIDDEN,
                &out))
        {
            DWORD dwError = GetLastError();
            DoTraceMessage(WppError, "CryptUnprotectData failed: %!WINERROR!", dwError);
            throw Win32Error(dwError, "CryptUnprotectData failed.");
        }

        CHeapPtr<BYTE, CLocalAllocator> pbData(out.pbData);
        auto data = reinterpret_cast<const std::byte*>(pbData.m_pData);
        std::vector<std::byte> plaintext(data, data + out.cbData);
        return plaintext;
    }

    std::vector<std::byte> Cryptography::ToBytes(const std::string& text)
    {
        auto data = reinterpret_cast<const std::byte*>(text.data());
        return std::vector<std::byte>(data, data + text.size());
    }

    std::string Cryptography::ToString(const std::vector<std::byte>& buffer)
    {
        return std::string(reinterpret_cast<const char*>(buffer.data()), buffer.size());
    }

    uint32_t Cryptography::CastToUInt32(size_t count)
    {
        if (count > std::numeric_limits<uint32_t>::max())
            throw std::length_error("Cryptography input exceeds uint32_t maximum.");

        return static_cast<uint32_t>(count);
    }
}
