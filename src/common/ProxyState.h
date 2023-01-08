//
// ProxyState.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once

namespace winpx
{
    enum class ProxyState
    {
        Unknown = 0,
        Starting,
        Running,
        Stopping,
        Stopped,
        Error,
    };
}
