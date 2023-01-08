//
// Unicode.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include <cstdint>
#include <string>
#include <string_view>

namespace win32
{
    class Unicode
    {
    public:
        static std::wstring MultiByteToWideChar(const char* text, uint32_t codePage);
        static std::wstring MultiByteToWideChar(const char* text, size_t chars, uint32_t codePage);
        static std::wstring MultiByteToWideChar(std::string_view text, uint32_t codePage);

        static std::string WideCharToMultiByte(const wchar_t* text, uint32_t codePage);
        static std::string WideCharToMultiByte(const wchar_t* text, size_t chars, uint32_t codePage);
        static std::string WideCharToMultiByte(std::wstring_view text, uint32_t codePage);

        static std::string ToUtf8(std::wstring_view text);
        static std::wstring FromUtf8(std::string_view text);

    private:
        static int CastToInt(size_t count);
    };
}
