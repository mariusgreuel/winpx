//
// WinPxApplication.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include "Options.h"
#include <common/ProxyConfiguration.h>

namespace winpx
{
    class WinPxApplication
    {
    public:
        WinPxApplication();
        ~WinPxApplication();

        WinPxApplication(const WinPxApplication&) = delete;
        WinPxApplication& operator=(const WinPxApplication&) = delete;

        int Run(int argc, wchar_t** argv);

    private:
        void ParseCommandLineArguments(int argc, wchar_t** argv);
        static std::string CollectServiceArguments(const ProxyConfiguration& proxyConfiguration);
        static void PrintBanner();
        static void PrintHelp();
        static void PrintLicenses();

    private:
        Options m_options;
        ProxyConfiguration m_proxyConfiguration;
    };
}
