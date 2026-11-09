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
        std::error_code error;
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
        asio::awaitable<ProxyResolverResult> ResolveProxyAsync(const std::string& url) const;

    private:
        asio::awaitable<ProxyResolverResult> GetProxyFromSystem(const std::string& url) const;
        asio::awaitable<ProxyResolverResult> GetProxyFromDirect(const std::string& url) const;
        asio::awaitable<ProxyResolverResult> GetProxyFromManual(const std::string& url) const;
        asio::awaitable<ProxyResolverResult> GetProxyFromAutoConfig(const std::string& url) const;
        asio::awaitable<ProxyResolverResult> GetProxyFromAutoDetect(const std::string& url) const;
        asio::awaitable<ProxyResolverResult> GetProxyFromOptions(DWORD dwFlags, const std::string& url, const std::string& autoConfigUrl) const;
        asio::awaitable<ProxyResolverResult> GetProxyFromOptions(DWORD dwFlags, const std::string& url, const std::string& autoConfigUrl, bool autoLogonIfChallenged) const;

        static void CALLBACK OnAsyncStatusCallback(HINTERNET handle, DWORD_PTR context, DWORD status, LPVOID statusInformation, DWORD statusInformationLength);

        void OpenSession();
        void CloseSession();
        std::shared_ptr<SystemProxyConfiguration> LoadSystemProxyConfiguration();
        void DumpEffectiveProxyConfiguration();
        static bool IsEqual(std::shared_ptr<SystemProxyConfiguration> old, const std::shared_ptr<SystemProxyConfiguration> current);

    private:
        const ProxyConfiguration& m_proxyConfiguration;
        std::atomic<std::shared_ptr<SystemProxyConfiguration>> m_systemProxyConfiguration;
        HINTERNET m_session = nullptr;
    };
}
