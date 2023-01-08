//
// WindowsFirewall.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include <filesystem>

namespace win32
{
    struct FirewallRule
    {
        std::string name;
        std::string description;
        std::string group;
        std::filesystem::path applicationPath;
        std::string localAddress;
        std::string remoteAddress;
        std::string localPorts;
        std::string remotePorts;
        NET_FW_IP_PROTOCOL protocol = NET_FW_IP_PROTOCOL_TCP;
        NET_FW_RULE_DIRECTION direction = NET_FW_RULE_DIR_IN;
        NET_FW_ACTION action = NET_FW_ACTION_ALLOW;
        long profiles = NET_FW_PROFILE2_ALL;
        bool enabled = true;
    };

    class WindowsFirewall
    {
    public:
        WindowsFirewall();
        ~WindowsFirewall();

        WindowsFirewall(const WindowsFirewall&) = delete;
        WindowsFirewall& operator=(const WindowsFirewall&) = delete;

        void AddRule(const FirewallRule& rule);
        void RemoveAllRules(std::string_view name);

    private:
        HRESULT m_hrCom = S_OK;
    };
}
