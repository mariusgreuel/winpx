//
// UrlLauncher.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include <string_view>

namespace win32
{
    class UrlLauncher
    {
    public:
        static std::error_code Launch(std::string_view url);
    };
}
