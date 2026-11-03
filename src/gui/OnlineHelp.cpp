//
// OnlineHelp.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include "pch.h"

#include "OnlineHelp.h"

#include <win32/GroupPolicy.h>
#include <win32/Registry.h>
#include <win32/UrlLauncher.h>

namespace winpx
{
    using namespace std::literals::string_view_literals;
    using namespace win32;

    void OnlineHelp::Open(std::string_view helpId)
    {
        static const std::unordered_map<std::string_view, std::string_view> helpUrls = {
            { "HelpUrlGeneral"sv, "https://github.com/mariusgreuel/winpx/wiki/Configuring-WinPX#general-tab"sv },
            { "HelpUrlGateway"sv, "https://github.com/mariusgreuel/winpx/wiki/Configuring-WinPX#gateway-tab"sv },
            { "HelpUrlSecurity"sv, "https://github.com/mariusgreuel/winpx/wiki/Configuring-WinPX#security-tab"sv },
            { "HelpUrlStatus"sv, "https://github.com/mariusgreuel/winpx/wiki/Configuring-WinPX#status-tab"sv },
            { "HelpUrlDiagnostics"sv, "https://github.com/mariusgreuel/winpx/wiki/Configuring-WinPX#diagnostics-tab"sv },
            { "HelpUrlAbout"sv, "https://github.com/mariusgreuel/winpx/wiki/Configuring-WinPX#about-tab"sv }
        };

        auto url = GroupPolicy::GetValue(helpId, Registry::GetValue(helpId, helpUrls.at(helpId)));
        UrlLauncher::Launch(url);
    }
}
