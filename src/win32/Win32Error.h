//
// Win32Error.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include <string>
#include <system_error>

namespace win32
{
    class Win32Error : public std::system_error
    {
    public:
        explicit Win32Error(int error, const std::string& what) :
            std::system_error(error, std::system_category(), what)
        {
        }
    };
}
