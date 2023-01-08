//
// WppTracing.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once

namespace win32
{
    class WppTracing
    {
    public:
        WppTracing();
        ~WppTracing();

        WppTracing(const WppTracing&) = delete;
        WppTracing& operator=(const WppTracing&) = delete;
    };
}
