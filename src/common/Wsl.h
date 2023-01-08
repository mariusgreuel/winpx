//
// Wsl.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include <istream>
#include <string>
#include <string_view>

namespace winpx
{
    class Wsl
    {
    public:
        static bool IsVersion2Used();
        static bool UsesNatNetworkingMode();
        static std::string GetNetworkingMode();
        static std::string GetVirtualSwitchAddress();

    private:
        static std::string ParseWslNetworkingMode(std::istream& stream);
        static std::string FindAdapterByFriendlyName(std::wstring_view name);
        static std::string GetFirstIPv4Address(PIP_ADAPTER_ADDRESSES pAdapter);
    };
}
