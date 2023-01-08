//
// Cryptography.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include <cstddef>
#include <string>
#include <vector>

namespace win32
{
    class Cryptography
    {
    public:
        static std::string ProtectData(const std::string& plaintext);
        static std::string UnprotectData(const std::string& ciphertext);

        static std::vector<std::byte> ProtectData(const std::vector<std::byte>& plaintext);
        static std::vector<std::byte> UnprotectData(const std::vector<std::byte>& ciphertext);

    private:
        static uint32_t CastToUInt32(size_t count);
        static std::vector<std::byte> ToBytes(const std::string& text);
        static std::string ToString(const std::vector<std::byte>& buffer);
    };
}
