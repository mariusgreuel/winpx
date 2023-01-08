//
// ProxyStatus.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include "ProxyState.h"

#include <string>

namespace winpx
{
    struct ProxyStatus
    {
        ProxyState m_state{};
        std::string m_status;
    };
}
