//
// Firewall.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include "pch.h"

#include "Firewall.h"
#include "Firewall.tmh"

#include <common/Wsl.h>
#include <win32/Environment.h>
#include <win32/RunAsAdmin.h>
#include <win32/WindowsFirewall.h>

namespace winpx
{
    using namespace win32;

    std::error_code Firewall::AddWslExemption(uint16_t port)
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: port=%u", port);

        if (Environment::IsLocalAdministrator())
        {
            return ConfigureFirewallForWsl(port);
        }
        else
        {
            return RunAsAdmin::Launch(Environment::GetProcessPath(), std::format("--port={} --configure-firewall", port));
        }
    }

    std::error_code Firewall::ConfigureFirewallForWsl(uint16_t port)
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: port=%u", port);

        try
        {
            FirewallRule rule;
            rule.name = "WinPX";
            rule.description = "Allow WinPX to accept incoming connections from WSL";
            rule.applicationPath = Environment::GetProcessPath();
            rule.localAddress = Wsl::GetVirtualSwitchAddress();
            rule.localPorts = std::to_string(port);
            rule.profiles = NET_FW_PROFILE2_PRIVATE | NET_FW_PROFILE2_PUBLIC;

            WindowsFirewall firewall;
            firewall.RemoveAllRules("winpx.exe");
            firewall.RemoveAllRules(rule.name);
            firewall.AddRule(rule);

            return {};
        }
        catch (const std::system_error& e)
        {
            DoTraceMessage(WppError, "Failed to add firewall rule: %s", e.what());
            return e.code();
        }
    }
}
