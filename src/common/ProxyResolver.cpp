//
// ProxyResolver.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include "pch.h"

#include "ProxyResolver.h"
#include "ProxyResolver.tmh"

#include "HttpTools.h"
#include "Tools.h"
#include "Uri.h"
#include <win32/Unicode.h>
#include <win32/Win32Error.h>

namespace winpx
{
    using namespace std::literals::string_view_literals;
    using namespace win32;

    ProxyResolver::ProxyResolver(const ProxyConfiguration& proxyConfiguration) :
        m_proxyConfiguration(proxyConfiguration)
    {
        DoTraceMessage(WppObject, "%!FUNC!");

        ReloadSystemSettings();
        OpenSession();
    }

    ProxyResolver::~ProxyResolver()
    {
        DoTraceMessage(WppObject, "%!FUNC!");

        CloseSession();
    }

    SystemProxyConfiguration ProxyResolver::GetSystemProxyConfiguration() const
    {
        auto systemProxyConfiguration = m_systemProxyConfiguration.load();
        return systemProxyConfiguration ? *systemProxyConfiguration : SystemProxyConfiguration{};
    }

    void ProxyResolver::ReloadSystemSettings()
    {
        DoTraceMessage(WppVerbose, "%!FUNC!");

        LoadSystemProxyConfiguration();
        DumpEffectiveProxyConfiguration();
    }

    ProxyResolverResult ProxyResolver::ResolveProxy(const std::string& url) const
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: url='%!str!', proxyMode=%!PROXY_MODE!", url, static_cast<int>(m_proxyConfiguration.proxyMode));

        switch (m_proxyConfiguration.proxyMode)
        {
        case ProxyMode::Default:
        case ProxyMode::System:
            return GetProxyFromSystem(url);
        case ProxyMode::Direct:
            return GetProxyFromDirect(url);
        case ProxyMode::Manual:
            return GetProxyFromManual(url);
        case ProxyMode::AutoConfig:
            return GetProxyFromAutoConfig(url);
        case ProxyMode::AutoDetect:
            return GetProxyFromAutoDetect(url);
        default:
            throw std::invalid_argument("Invalid proxy mode.");
        }
    }

    ProxyResolverResult ProxyResolver::GetProxyFromSystem(const std::string& url) const
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: url='%!str!'", url);

        auto systemProxyConfiguration = m_systemProxyConfiguration.load();

        if (!systemProxyConfiguration->autoConfigUrl.empty())
        {
            auto autoConfigUrlW = Unicode::FromUtf8(systemProxyConfiguration->autoConfigUrl);

            WINHTTP_AUTOPROXY_OPTIONS options{};
            options.dwFlags = WINHTTP_AUTOPROXY_CONFIG_URL;
            options.lpszAutoConfigUrl = autoConfigUrlW.c_str();
            return GetProxyFromOptions(url, options);
        }
        else if (!systemProxyConfiguration->proxy.empty())
        {
            Uri proxy(HttpTools::PickProxyFromList(systemProxyConfiguration->proxy, url));
            DoTraceMessage(WppTrace, "Using system proxy '%!sv!:%d' for '%!str!'", proxy.host, proxy.port, url);
            return { proxy.IsEmpty(), proxy.GetHost(), proxy.GetPort() };
        }
        else if (systemProxyConfiguration->autoDetect)
        {
            WINHTTP_AUTOPROXY_OPTIONS options{};
            options.dwFlags = WINHTTP_AUTOPROXY_AUTO_DETECT;
            options.dwAutoDetectFlags = WINHTTP_AUTO_DETECT_TYPE_DHCP | WINHTTP_AUTO_DETECT_TYPE_DNS_A;
            return GetProxyFromOptions(url, options);
        }
        else
        {
            DoTraceMessage(WppTrace, "No system proxy configured for '%!str!', using DIRECT.", url);
            return {};
        }
    }

    ProxyResolverResult ProxyResolver::GetProxyFromDirect(const std::string& url) const
    {
        DoTraceMessage(WppTrace, "No proxy configured for '%!str!', using DIRECT.", url);
        return {};
    }

    ProxyResolverResult ProxyResolver::GetProxyFromManual(const std::string& url) const
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: url='%!str!', httpProxyUrl='%!str!', httpsProxyUrl='%!str!'",
            url,
            m_proxyConfiguration.httpProxyUrl,
            m_proxyConfiguration.httpsProxyUrl);

        bool isHttps = IStartsWith(url, "https:"sv);
        const auto& primary = isHttps ? m_proxyConfiguration.httpsProxyUrl : m_proxyConfiguration.httpProxyUrl;
        const auto& fallback = isHttps ? m_proxyConfiguration.httpProxyUrl : m_proxyConfiguration.httpsProxyUrl;
        const auto& proxy = !primary.empty() ? primary : fallback;

        if (proxy.empty())
        {
            DoTraceMessage(WppWarning, "No manual proxy configured for '%!str!', using DIRECT.", url);
            return {};
        }

        Uri uri(proxy);
        DoTraceMessage(WppTrace, "Using manual proxy '%!sv!:%d' for '%!str!'", uri.host, uri.port, url);
        return { uri.IsEmpty(), uri.GetHost(), uri.GetPort() };
    }

    ProxyResolverResult ProxyResolver::GetProxyFromAutoConfig(const std::string& url) const
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: url='%!str!', autoConfigUrl='%!str!'", url, m_proxyConfiguration.autoConfigUrl);

        if (m_proxyConfiguration.autoConfigUrl.empty())
        {
            DoTraceMessage(WppWarning, "No PAC URL configured for '%!str!', using DIRECT.", url);
            return {};
        }

        auto autoConfigUrlW = Unicode::FromUtf8(m_proxyConfiguration.autoConfigUrl);

        WINHTTP_AUTOPROXY_OPTIONS options{};
        options.dwFlags = WINHTTP_AUTOPROXY_CONFIG_URL;
        options.lpszAutoConfigUrl = autoConfigUrlW.c_str();
        return GetProxyFromOptions(url, options);
    }

    ProxyResolverResult ProxyResolver::GetProxyFromAutoDetect(const std::string& url) const
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: url='%!str!'", url);

        WINHTTP_AUTOPROXY_OPTIONS options{};
        options.dwFlags = WINHTTP_AUTOPROXY_AUTO_DETECT;
        options.dwAutoDetectFlags = WINHTTP_AUTO_DETECT_TYPE_DHCP | WINHTTP_AUTO_DETECT_TYPE_DNS_A;
        return GetProxyFromOptions(url, options);
    }

    ProxyResolverResult ProxyResolver::GetProxyFromOptions(const std::string& url, WINHTTP_AUTOPROXY_OPTIONS& options) const
    {
        options.fAutoLogonIfChallenged = TRUE;

        std::string host;
        uint16_t port = 0;

        WINHTTP_PROXY_INFO proxyInfo{};
        if (!WinHttpGetProxyForUrl(m_session, Unicode::FromUtf8(url).c_str(), &options, &proxyInfo))
        {
            DWORD dwError = GetLastError();
            DoTraceMessage(WppError, "WinHttpGetProxyForUrl failed: %!WINERROR!", dwError);
        }
        else if (proxyInfo.dwAccessType == WINHTTP_ACCESS_TYPE_NO_PROXY)
        {
            DoTraceMessage(WppTrace, "Using DIRECT connection for '%!str!'", url);
        }
        else if (proxyInfo.lpszProxy != nullptr)
        {
            auto proxy = HttpTools::PickProxyFromList(Unicode::ToUtf8(proxyInfo.lpszProxy), url);

            Uri uri(proxy);
            host = uri.GetHost();
            port = uri.GetPort();
            DoTraceMessage(WppTrace, "Resolved proxy '%!sv!:%d' for '%!str!'", host, port, url);
        }

        if (proxyInfo.lpszProxy != nullptr)
        {
            GlobalFree(proxyInfo.lpszProxy);
        }

        if (proxyInfo.lpszProxyBypass != nullptr)
        {
            GlobalFree(proxyInfo.lpszProxyBypass);
        }

        return { host.empty(), host, port };
    }

    void ProxyResolver::OpenSession()
    {
        DoTraceMessage(WppVerbose, "%!FUNC!");

        CloseSession();

        m_session = WinHttpOpen(
            Unicode::FromUtf8(m_proxyConfiguration.userAgent).c_str(),
            WINHTTP_ACCESS_TYPE_NO_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0);
        if (!m_session)
        {
            DWORD dwError = GetLastError();
            DoTraceMessage(WppError, "WinHttpOpen failed: %!WINERROR!", dwError);
            throw Win32Error(dwError, "Failed to open WinHTTP session.");
        }
    }

    void ProxyResolver::CloseSession()
    {
        DoTraceMessage(WppVerbose, "%!FUNC!");

        if (m_session != nullptr)
        {
            WinHttpCloseHandle(m_session);
            m_session = nullptr;
        }
    }

    void ProxyResolver::LoadSystemProxyConfiguration()
    {
        DoTraceMessage(WppVerbose, "%!FUNC!");

        auto systemProxyConfiguration = std::make_shared<SystemProxyConfiguration>();

        WINHTTP_CURRENT_USER_IE_PROXY_CONFIG config{};
        BOOL success = WinHttpGetIEProxyConfigForCurrentUser(&config);
        if (success)
        {
            systemProxyConfiguration->autoDetect = config.fAutoDetect != FALSE;

            if (config.fAutoDetect)
            {
                DoTraceMessage(WppVerbose, "System proxy set to auto-detect.");
            }

            if (config.lpszAutoConfigUrl != nullptr)
            {
                systemProxyConfiguration->autoConfigUrl = Unicode::ToUtf8(config.lpszAutoConfigUrl);
                DoTraceMessage(WppVerbose, "System proxy PAC URL: '%!str!'", systemProxyConfiguration->autoConfigUrl);
            }

            if (config.lpszProxy != nullptr)
            {
                systemProxyConfiguration->proxy = Unicode::ToUtf8(config.lpszProxy);
                DoTraceMessage(WppVerbose, "System proxy URL: '%!str!'", systemProxyConfiguration->proxy);
            }

            if (config.lpszProxyBypass != nullptr)
            {
                systemProxyConfiguration->proxyBypass = Unicode::ToUtf8(config.lpszProxyBypass);
                DoTraceMessage(WppVerbose, "System proxy bypass list: '%!str!'", systemProxyConfiguration->proxyBypass);
            }
        }
        else
        {
            DWORD dwError = GetLastError();
            DoTraceMessage(WppWarning, "WinHttpGetIEProxyConfigForCurrentUser failed: %!WINERROR!", dwError);
        }

        if (config.lpszAutoConfigUrl != nullptr)
        {
            GlobalFree(config.lpszAutoConfigUrl);
        }

        if (config.lpszProxy != nullptr)
        {
            GlobalFree(config.lpszProxy);
        }

        if (config.lpszProxyBypass != nullptr)
        {
            GlobalFree(config.lpszProxyBypass);
        }

        m_systemProxyConfiguration.store(systemProxyConfiguration);
    }

    void ProxyResolver::DumpEffectiveProxyConfiguration()
    {
        auto systemProxyConfiguration = m_systemProxyConfiguration.load();

        switch (m_proxyConfiguration.proxyMode)
        {
        case ProxyMode::Default:
        case ProxyMode::System:
            if (!systemProxyConfiguration->autoConfigUrl.empty())
            {
                DoTraceMessage(WppTrace, "Proxy mode set to 'System', using PAC URL '%!str!'.",
                    systemProxyConfiguration->autoConfigUrl);
            }
            else if (!systemProxyConfiguration->proxy.empty())
            {
                DoTraceMessage(WppTrace, "Proxy mode set to 'System', using proxy URL '%!str!'.",
                    systemProxyConfiguration->proxy);
            }
            else if (systemProxyConfiguration->autoDetect)
            {
                DoTraceMessage(WppTrace, "Proxy mode set to 'System', using WPAD.");
            }
            else
            {
                DoTraceMessage(WppTrace, "Proxy mode set to 'System', using 'DIRECT'.");
            }
            break;
        case ProxyMode::Direct:
            DoTraceMessage(WppTrace, "Proxy mode set to 'Direct', using DIRECT.");
            break;
        case ProxyMode::Manual:
            DoTraceMessage(WppTrace, "Proxy mode set to 'Manual', using proxy URL HTTP='%!str!' and HTTPS='%!str!'.",
                m_proxyConfiguration.httpProxyUrl,
                m_proxyConfiguration.httpsProxyUrl);
            break;
        case ProxyMode::AutoConfig:
            DoTraceMessage(WppTrace, "Proxy mode set to 'AutoConfig', using PAC URL '%!str!'.",
                m_proxyConfiguration.autoConfigUrl);
            break;
        case ProxyMode::AutoDetect:
            DoTraceMessage(WppTrace, "Proxy mode set to 'AutoDetect', using WPAD.");
            break;
        default:
            throw std::invalid_argument("Invalid proxy mode.");
        }
    }
}
