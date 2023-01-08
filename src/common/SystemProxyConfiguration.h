//
// SystemProxyConfiguration.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include <string>

namespace winpx
{
    struct SystemProxyConfiguration
    {
        std::string autoConfigUrl;
        std::string proxy;
        std::string proxyBypass;
        bool autoDetect = false;
    };
}
