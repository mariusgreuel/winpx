//
// Firewall.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once

namespace winpx
{
    class Firewall
    {
    public:
        static std::error_code AddWslExemption(uint16_t port);
        static std::error_code ConfigureFirewallForWsl(uint16_t port);
    };
}
