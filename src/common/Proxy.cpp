//
// Proxy.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include "pch.h"

#include "Proxy.h"
#include "Proxy.tmh"

#include <win32/Environment.h>
#include <win32/MitigationPolicy.h>

namespace winpx
{
    using namespace std::literals::string_view_literals;
    using namespace win32;

    Proxy::Proxy(const ProxyConfiguration& proxyConfiguration) :
        m_proxyConfiguration(proxyConfiguration),
        m_acceptorV4(m_ioContext),
        m_acceptorV6(m_ioContext),
        m_acceptorCustom(m_ioContext),
        m_proxyResolver(proxyConfiguration),
        m_proxyRequestHandler(proxyConfiguration, m_proxyResolver, m_proxyStatistics)
    {
        DoTraceMessage(WppObject, "%!FUNC!");

        MitigationPolicy::ApplyToProcess();
    }

    Proxy::~Proxy()
    {
        DoTraceMessage(WppObject, "%!FUNC!");

        if (m_state != ProxyState::Stopped)
        {
            Stop();
        }
    }

    const ProxyConfiguration& Proxy::GetProxyConfiguration() const noexcept
    {
        return m_proxyConfiguration;
    }

    const ProxyResolver& Proxy::GetProxyResolver() const noexcept
    {
        return m_proxyResolver;
    }

    const ProxyStatistics& Proxy::GetProxyStatistics() const noexcept
    {
        return m_proxyStatistics;
    }

    ProxyState Proxy::GetState() const noexcept
    {
        return m_state;
    }

    std::error_code Proxy::GetErrorCode() const noexcept
    {
        return m_errorCode;
    }

    std::string Proxy::FormatProxyConfiguration() const
    {
        std::string result = "Proxy Mode: ";

        switch (m_proxyConfiguration.proxyMode)
        {
        case ProxyMode::Default:
        case ProxyMode::System:
        {
            auto systemProxyConfiguration = m_proxyResolver.GetSystemProxyConfiguration();
            if (systemProxyConfiguration.autoDetect)
            {
                result += "System / Web Proxy Auto-Discovery (WPAD)\r\n";
            }
            else if (!systemProxyConfiguration.autoConfigUrl.empty())
            {
                result += "System / Proxy Auto-Config (PAC)\r\n";
            }
            else if (!systemProxyConfiguration.proxy.empty())
            {
                result += "System / Manual\r\n";
            }
            else
            {
                result += "System / Direct\r\n";
            }
            break;
        }
        case ProxyMode::Direct:
            result += "Direct\r\n";
            break;
        case ProxyMode::Manual:
            result += "Manual\r\n";
            break;
        case ProxyMode::AutoConfig:
            result += "Proxy Auto-Config (PAC)\r\n";
            break;
        case ProxyMode::AutoDetect:
            result += "Web Proxy Auto-Discovery (WPAD)\r\n";
            break;
        default:
            throw std::invalid_argument("Invalid proxy mode.");
        }

        return result;
    }

    std::string Proxy::FormatProxyUrls() const
    {
        std::string result;

        switch (m_proxyConfiguration.proxyMode)
        {
        case ProxyMode::Default:
        case ProxyMode::System:
        {
            auto systemProxyConfiguration = m_proxyResolver.GetSystemProxyConfiguration();
            if (!systemProxyConfiguration.autoConfigUrl.empty())
            {
                result += std::format("PAC URL: {}\r\n", systemProxyConfiguration.autoConfigUrl);
            }
            else if (!systemProxyConfiguration.proxy.empty())
            {
                result += std::format("Upstream Proxy: {}\r\n", systemProxyConfiguration.proxy);
            }
            break;
        }
        case ProxyMode::AutoConfig:
            result += std::format("PAC URL: {}\r\n", m_proxyConfiguration.autoConfigUrl);
            break;
        case ProxyMode::Manual:
            result += std::format("Upstream HTTP Proxy: {}\r\n", m_proxyConfiguration.httpProxyUrl);
            result += std::format("Upstream HTTPS Proxy: {}\r\n", m_proxyConfiguration.httpsProxyUrl);
            break;
        }

        return result;
    }

    void Proxy::Start()
    {
        DoTraceMessage(WppInfo, "%!FUNC!: proxyMode=%!PROXY_MODE!, networkMode=%!NETWORK_MODE!",
            static_cast<int>(m_proxyConfiguration.proxyMode),
            static_cast<int>(m_proxyConfiguration.networkMode));

        m_state = ProxyState::Starting;

        if (m_proxyConfiguration.setEnvironmentVariables)
        {
            SetHttpProxyVariables();
        }

        try
        {
            auto address = m_proxyConfiguration.networkMode == NetworkMode::Any ? asio::ip::address_v4::any() : asio::ip::address_v4::loopback();
            auto endpoint = asio::ip::tcp::endpoint(address, m_proxyConfiguration.port);
            DoTraceMessage(WppVerbose, "IPv4 address: %!str!:%u", endpoint.address().to_string(), endpoint.port());
            m_acceptorV4.open(endpoint.protocol());
            if (m_proxyConfiguration.reuseSocketAddresses)
                m_acceptorV4.set_option(asio::socket_base::reuse_address(true));
            m_acceptorV4.bind(endpoint);
            m_acceptorV4.listen();

            asio::co_spawn(m_ioContext, Listen(m_acceptorV4, "IPv4"), asio::detached);
        }
        catch (const std::system_error& e)
        {
            DoTraceMessage(WppError, "Failed to create IPv4 acceptor: code=%d, %s", e.code().value(), e.what());
            m_state = ProxyState::Error;
            m_errorCode = e.code();
        }

        try
        {
            auto address = m_proxyConfiguration.networkMode == NetworkMode::Any ? asio::ip::address_v6::any() : asio::ip::address_v6::loopback();
            auto endpoint = asio::ip::tcp::endpoint(address, m_proxyConfiguration.port);
            DoTraceMessage(WppVerbose, "IPv6 address: [%!str!]:%u", endpoint.address().to_string(), endpoint.port());
            m_acceptorV6.open(endpoint.protocol());
            if (m_proxyConfiguration.reuseSocketAddresses)
                m_acceptorV6.set_option(asio::socket_base::reuse_address(true));
            m_acceptorV6.bind(endpoint);
            m_acceptorV6.listen();

            asio::co_spawn(m_ioContext, Listen(m_acceptorV6, "IPv6"), asio::detached);
        }
        catch (const std::system_error& e)
        {
            DoTraceMessage(WppError, "Failed to create IPv6 acceptor: code=%d, %s", e.code().value(), e.what());
            m_state = ProxyState::Error;
            m_errorCode = e.code();
        }

        if (!m_proxyConfiguration.customAddress.empty())
        {
            try
            {
                asio::error_code ec;
                auto address = asio::ip::make_address(m_proxyConfiguration.customAddress, ec);
                if (!ec)
                {
                    auto endpoint = asio::ip::tcp::endpoint(address, m_proxyConfiguration.port);
                    DoTraceMessage(WppVerbose, "Custom address: %!str!:%u", endpoint.address().to_string(), endpoint.port());
                    m_acceptorCustom.open(endpoint.protocol());
                    m_acceptorCustom.set_option(asio::socket_base::reuse_address(m_proxyConfiguration.reuseSocketAddresses));
                    m_acceptorCustom.bind(endpoint);
                    m_acceptorCustom.listen();

                    asio::co_spawn(m_ioContext, Listen(m_acceptorCustom, "Custom"), asio::detached);
                }
            }
            catch (const std::system_error& e)
            {
                DoTraceMessage(WppError, "Failed to create custom acceptor: code=%d, %s", e.code().value(), e.what());
                m_state = ProxyState::Error;
                m_errorCode = e.code();
            }
        }

        try
        {
            if (m_state == ProxyState::Starting)
            {
                CreateThreadPool(GetThreadPoolSize());
                m_state = ProxyState::Running;
            }
        }
        catch (const std::system_error& e)
        {
            DoTraceMessage(WppError, "Failed to create thread pool: code=%d, %s", e.code().value(), e.what());
            m_state = ProxyState::Error;
            m_errorCode = e.code();
        }
        catch (const std::exception& e)
        {
            DoTraceMessage(WppError, "Failed to create thread pool: %s", e.what());
            m_state = ProxyState::Error;
            m_errorCode = std::error_code(EDOM, std::generic_category());
        }
    }

    void Proxy::Stop()
    {
        DoTraceMessage(WppInfo, "%!FUNC!");

        if (m_state == ProxyState::Stopped)
            return;

        try
        {
            m_state = ProxyState::Stopping;

            asio::error_code ec;
            m_acceptorV4.close(ec);
            m_acceptorV6.close(ec);
            m_acceptorCustom.close(ec);
            m_ioContext.stop();

            ShutdownThreadPool();

            m_state = ProxyState::Stopped;
        }
        catch (const std::system_error& e)
        {
            DoTraceMessage(WppError, "Exception: code=%d, %s", e.code().value(), e.what());
            m_state = ProxyState::Error;
            m_errorCode = e.code();
        }
        catch (const std::exception& e)
        {
            DoTraceMessage(WppError, "Exception: %s", e.what());
            m_state = ProxyState::Error;
            m_errorCode = std::error_code(EDOM, std::generic_category());
        }
    }

    void Proxy::WaitForIoShutdown()
    {
        DoTraceMessage(WppInfo, "%!FUNC!");

        asio::signal_set signals(m_ioContext, SIGINT, SIGTERM);
        signals.async_wait([&](const asio::error_code&, int) { m_ioContext.stop(); });

        m_ioContext.run();
    }

    ProxyResolverResult Proxy::ResolveProxy(const std::string& url)
    {
        return m_proxyResolver.ResolveProxy(url);
    }

    void Proxy::SetHttpProxyVariables()
    {
        DoTraceMessage(WppVerbose, "%!FUNC!");

        auto proxyUrl = m_proxyConfiguration.GetWinPxProxyUrl();

        bool changed = false;
        changed |= Environment::SetUserVariable("http_proxy", proxyUrl) == S_OK;
        changed |= Environment::SetUserVariable("https_proxy", proxyUrl) == S_OK;
        if (changed)
        {
            Environment::SendSettingChangeNotification();
        }
    }

    void Proxy::RemoveHttpProxyVariables()
    {
        DoTraceMessage(WppVerbose, "%!FUNC!");

        bool changed = false;
        changed |= Environment::RemoveUserVariable("http_proxy"sv) == S_OK;
        changed |= Environment::RemoveUserVariable("https_proxy"sv) == S_OK;
        if (changed)
        {
            Environment::SendSettingChangeNotification();
        }
    }

    void Proxy::OnSystemSettingsChange()
    {
        DoTraceMessage(WppVerbose, "%!FUNC!");

        m_proxyResolver.ReloadSystemSettings();
    }

    asio::awaitable<void> Proxy::Listen(asio::ip::tcp::acceptor& acceptor, const char* who)
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: %s", who);

        while (true)
        {
            auto [ec, socket] = co_await acceptor.async_accept(asio::as_tuple(asio::use_awaitable));
            if (!ec)
            {
                asio::co_spawn(asio::make_strand(acceptor.get_executor()), m_proxyRequestHandler.Handle(std::move(socket)), asio::detached);
            }
            else if (ec == asio::error::operation_aborted)
            {
                break;
            }
            else
            {
                DoTraceMessage(WppWarning, "async_accept failed: %!str!", ec.message());
                asio::steady_timer t(co_await asio::this_coro::executor);
                t.expires_after(std::chrono::milliseconds(100));
                co_await t.async_wait(asio::use_awaitable);
            }
        }
    }

    uint32_t Proxy::GetThreadPoolSize() const
    {
        const uint32_t minThreads = 2;
        const uint32_t maxThreads = std::thread::hardware_concurrency();

        if (m_proxyConfiguration.threadPoolSize >= 1)
        {
            return std::max<uint32_t>(std::min<uint32_t>(m_proxyConfiguration.threadPoolSize, maxThreads), minThreads);
        }
        else if (m_proxyConfiguration.threadPoolSize <= -1)
        {
            uint32_t reservedThreads = static_cast<uint32_t>(-m_proxyConfiguration.threadPoolSize);
            return std::max<uint32_t>(reservedThreads < maxThreads ? maxThreads - reservedThreads : 2u, minThreads);
        }
        else
        {
            return std::max<uint32_t>(maxThreads, minThreads);
        }
    }

    void Proxy::CreateThreadPool(uint32_t threadCount)
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: threadCount=%u", threadCount);

        m_threadPool.reserve(threadCount);
        for (uint32_t i = 0; i < threadCount; i++)
        {
            m_threadPool.emplace_back([this] { m_ioContext.run(); });
        }
    }

    void Proxy::ShutdownThreadPool()
    {
        DoTraceMessage(WppVerbose, "%!FUNC!");

        for (auto& thread : m_threadPool)
        {
            thread.join();
        }

        m_threadPool.clear();
    }
}
