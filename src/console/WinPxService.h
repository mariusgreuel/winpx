//
// WinPxService.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include <common/Proxy.h>
#include <win32/Service.h>

namespace winpx
{
    class WinPxService : public win32::Service
    {
    public:
        WinPxService();
        ~WinPxService();

        WinPxService(const WinPxService&) = delete;
        WinPxService& operator=(const WinPxService&) = delete;

        void Start() override;
        void Run() override;
        void Stop() override;

    private:
        void ParseCommandLineArguments();

    private:
        ProxyConfiguration m_proxyConfiguration;
        std::unique_ptr<Proxy> m_proxy;
    };
}
