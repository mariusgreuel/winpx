//
// RunAsAdmin.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include <filesystem>
#include <string>

namespace win32
{
    class RunAsAdmin
    {
    public:
        static std::error_code Launch(const std::filesystem::path& exe, const std::string& arguments);
    };
}
