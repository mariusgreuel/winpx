//
// ProxyResolver.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include "ProxyConfiguration.h"
#include "SystemProxyConfiguration.h"

namespace winpx
{
    struct ProxyResolverResult
    {
        bool direct = true;
        std::string host;
        uint16_t port = 0;
    };

    class ProxyResolver
    {
    public:
        explicit ProxyResolver(const ProxyConfiguration& proxyConfiguration);
        ~ProxyResolver();

        ProxyResolver(const ProxyResolver&) = delete;
        ProxyResolver& operator=(const ProxyResolver&) = delete;

        SystemProxyConfiguration GetSystemProxyConfiguration() const;

        void ReloadSystemSettings();
        ProxyResolverResult ResolveProxy(const std::string& url) const;

    private:
        ProxyResolverResult GetProxyFromSystem(const std::string& url) const;
        ProxyResolverResult GetProxyFromDirect(const std::string& url) const;
        ProxyResolverResult GetProxyFromManual(const std::string& url) const;
        ProxyResolverResult GetProxyFromAutoConfig(const std::string& url) const;
        ProxyResolverResult GetProxyFromAutoDetect(const std::string& url) const;
        ProxyResolverResult GetProxyFromOptions(const std::string& url, WINHTTP_AUTOPROXY_OPTIONS& options) const;

        void OpenSession();
        void CloseSession();
        void LoadSystemProxyConfiguration();
        void DumpEffectiveProxyConfiguration();

    private:
        const ProxyConfiguration& m_proxyConfiguration;
        std::atomic<std::shared_ptr<SystemProxyConfiguration>> m_systemProxyConfiguration;
        HINTERNET m_session = nullptr;
    };
}
