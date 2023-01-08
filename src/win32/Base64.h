//
// Base64.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace win32
{
    class Base64
    {
    public:
        static std::string Encode(std::string_view buffer);

        static std::string Encode(const std::vector<std::byte>& buffer);
        static std::vector<std::byte> Decode(std::string_view buffer);

        static std::string Encode(const std::byte* buffer, size_t count);
        static std::vector<std::byte> Decode(const char* buffer, size_t count);
    };
}
