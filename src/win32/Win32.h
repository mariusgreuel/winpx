//
// Win32.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include <filesystem>
#include <string>

namespace win32
{
    class Win32
    {
    public:
        static std::string FormatErrorCode(uint32_t error);
        static std::string FormatErrorCode(std::error_code code);
        static std::filesystem::path GetKnownFolderPath(const KNOWNFOLDERID& rfid);
        static std::filesystem::path GetModulePath(HMODULE hModule);
    };
}
