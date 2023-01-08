//
// Proxy.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include "ProxyConfiguration.h"
#include "ProxyRequestHandler.h"
#include "ProxyResolver.h"
#include "ProxyState.h"
#include "ProxyStatistics.h"

namespace winpx
{
    class Proxy
    {
    public:
        explicit Proxy(const ProxyConfiguration& configuration);
        ~Proxy();

        Proxy(const Proxy&) = delete;
        Proxy& operator=(const Proxy&) = delete;

        const ProxyConfiguration& GetProxyConfiguration() const noexcept;
        const ProxyResolver& GetProxyResolver() const noexcept;
        const ProxyStatistics& GetProxyStatistics() const noexcept;

        ProxyState GetState() const noexcept;
        std::error_code GetErrorCode() const noexcept;

        std::string FormatProxyConfiguration() const;
        std::string FormatProxyUrls() const;

        void Start();
        void Stop();
        void WaitForIoShutdown();
        void OnSystemSettingsChange();

        ProxyResolverResult ResolveProxy(const std::string& url);
        void SetHttpProxyVariables();
        void RemoveHttpProxyVariables();


    private:
        asio::awaitable<void> Listen(asio::ip::tcp::acceptor& acceptor, const char* who);

        uint32_t GetThreadPoolSize() const;
        void CreateThreadPool(uint32_t threadCount);
        void ShutdownThreadPool();

    private:
        const ProxyConfiguration& m_proxyConfiguration;
        ProxyStatistics m_proxyStatistics;
        ProxyResolver m_proxyResolver;
        ProxyRequestHandler m_proxyRequestHandler;
        std::vector<std::thread> m_threadPool;
        asio::io_context m_ioContext;
        asio::ip::tcp::acceptor m_acceptorV4;
        asio::ip::tcp::acceptor m_acceptorV6;
        asio::ip::tcp::acceptor m_acceptorCustom;
        ProxyState m_state = ProxyState::Unknown;
        std::error_code m_errorCode;
    };
}
