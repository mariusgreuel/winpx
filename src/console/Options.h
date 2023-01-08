//
// Options.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once

namespace winpx
{
    struct Options
    {
        bool run = false;
        bool installService = false;
        bool uninstallService = false;
        bool showLicenses = false;
        bool showHelp = false;
        std::string serviceName = "WinPX";
        std::string displayName = "WinPX Proxy Server";
    };
}
