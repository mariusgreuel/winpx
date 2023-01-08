//
// ProxyConfiguration.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once

namespace winpx
{
    enum class ProxyMode
    {
        Default,
        System,
        Direct,
        Manual,
        AutoConfig,
        AutoDetect,
    };
}
