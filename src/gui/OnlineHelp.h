//
// OnlineHelp.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include <string_view>

namespace winpx
{
    class OnlineHelp
    {
    public:
        static void Open(std::string_view helpId);
    };
}
