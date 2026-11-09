//
// ProxyResolver.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include "pch.h"

#include "ProxyResolver.h"
#include "ProxyResolver.tmh"

#include "HttpHelper.h"
#include "StringHelper.h"
#include "Uri.h"
#include <win32/Logger.h>
#include <win32/Unicode.h>
#include <win32/Win32Error.h>

namespace winpx
{
    using namespace std::literals::string_view_literals;
    using namespace win32;

    namespace
    {
        struct AsyncProxyRequest
        {
            HINTERNET resolver = nullptr;
            std::wstring url;
            std::wstring autoConfigUrl;
            WINHTTP_AUTOPROXY_OPTIONS options{};
            std::function<void(ProxyResolverResult)> complete;

            ~AsyncProxyRequest()
            {
                if (resolver != nullptr)
                {
                    WinHttpCloseHandle(resolver);
                }
            }
        };

        ProxyResolverResult GetResultFromResolver(const AsyncProxyRequest& request)
        {
            WINHTTP_PROXY_RESULT proxyResult{};
            DWORD dwError = WinHttpGetProxyResult(request.resolver, &proxyResult);
            if (dwError != ERROR_SUCCESS)
            {
                DoTraceMessage(WppError, "WinHttpGetProxyResult failed: %!WINERROR!", dwError);
                return ProxyResolverResult{ std::error_code(dwError, std::system_category()) };
            }

            ProxyResolverResult result;
            for (DWORD i = 0; i < proxyResult.cEntries; i++)
            {
                const auto& entry = proxyResult.pEntries[i];
                if (entry.fProxy && entry.pwszProxy != nullptr)
                {
                    result.direct = false;
                    result.host = Unicode::ToUtf8(entry.pwszProxy);
                    result.port = entry.ProxyPort;
                    DoTraceMessage(WppVerbose, "Resolved proxy '%!str!:%d' for '%!wstr!'", result.host, result.port, request.url);
                    break;
                }
            }

            if (result.direct)
            {
                DoTraceMessage(WppVerbose, "Using DIRECT connection for '%!wstr!'", request.url);
            }

            WinHttpFreeProxyResult(&proxyResult);
            return result;
        }
    }

    ProxyResolver::ProxyResolver(const ProxyConfiguration& proxyConfiguration) :
        m_proxyConfiguration(proxyConfiguration)
    {
        DoTraceMessage(WppObject, "%!FUNC!");

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

        auto systemProxyConfiguration = LoadSystemProxyConfiguration();
        if (!IsEqual(systemProxyConfiguration, m_systemProxyConfiguration.load()))
        {
            m_systemProxyConfiguration.store(systemProxyConfiguration);
            DumpEffectiveProxyConfiguration();
        }
        else
        {
            DoTraceMessage(WppVerbose, "System proxy configuration unchanged, skipping update.");
        }
    }

    ProxyResolverResult ProxyResolver::ResolveProxy(const std::string& url) const
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: url='%!str!', proxyMode=%!ProxyMode!", url, static_cast<int>(m_proxyConfiguration.proxyMode));

        asio::io_context ioContext;
        auto future = asio::co_spawn(ioContext, ResolveProxyAsync(url), asio::use_future);
        ioContext.run();
        return future.get();
    }

    asio::awaitable<ProxyResolverResult> ProxyResolver::ResolveProxyAsync(const std::string& url) const
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: url='%!str!', proxyMode=%!ProxyMode!", url, static_cast<int>(m_proxyConfiguration.proxyMode));

        switch (m_proxyConfiguration.proxyMode)
        {
        case ProxyMode::Default:
        case ProxyMode::System:
            co_return co_await GetProxyFromSystem(url);
        case ProxyMode::Direct:
            co_return co_await GetProxyFromDirect(url);
        case ProxyMode::Manual:
            co_return co_await GetProxyFromManual(url);
        case ProxyMode::AutoConfig:
            co_return co_await GetProxyFromAutoConfig(url);
        case ProxyMode::AutoDetect:
            co_return co_await GetProxyFromAutoDetect(url);
        default:
            throw std::invalid_argument("Invalid proxy mode.");
        }
    }

    asio::awaitable<ProxyResolverResult> ProxyResolver::GetProxyFromSystem(const std::string& url) const
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: url='%!str!'", url);

        auto systemProxyConfiguration = m_systemProxyConfiguration.load();
        if (!systemProxyConfiguration->autoConfigUrl.empty())
        {
            co_return co_await GetProxyFromOptions(WINHTTP_AUTOPROXY_CONFIG_URL, url, systemProxyConfiguration->autoConfigUrl);
        }
        else if (systemProxyConfiguration->autoDetect && systemProxyConfiguration->proxy.empty())
        {
            co_return co_await GetProxyFromOptions(WINHTTP_AUTOPROXY_AUTO_DETECT, url, {});
        }
        else if (!systemProxyConfiguration->proxy.empty())
        {
            Uri proxy(PickProxyFromList(systemProxyConfiguration->proxy, url));
            DoTraceMessage(WppVerbose, "Using system proxy '%!sv!:%d' for '%!str!'", proxy.host, proxy.port, url);
            co_return ProxyResolverResult{ {}, proxy.IsEmpty(), proxy.GetHost(), proxy.GetPort() };
        }
        else
        {
            DoTraceMessage(WppVerbose, "No system proxy configured for '%!str!', using DIRECT.", url);
            co_return ProxyResolverResult{};
        }
    }

    asio::awaitable<ProxyResolverResult> ProxyResolver::GetProxyFromDirect(const std::string& url) const
    {
        DoTraceMessage(WppVerbose, "No proxy configured for '%!str!', using DIRECT.", url);
        co_return ProxyResolverResult{};
    }

    asio::awaitable<ProxyResolverResult> ProxyResolver::GetProxyFromManual(const std::string& url) const
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
            co_return ProxyResolverResult{};
        }

        Uri uri(proxy);
        DoTraceMessage(WppVerbose, "Using manual proxy '%!sv!:%d' for '%!str!'", uri.host, uri.port, url);
        co_return ProxyResolverResult{ {}, uri.IsEmpty(), uri.GetHost(), uri.GetPort() };
    }

    asio::awaitable<ProxyResolverResult> ProxyResolver::GetProxyFromAutoConfig(const std::string& url) const
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: url='%!str!', autoConfigUrl='%!str!'", url, m_proxyConfiguration.autoConfigUrl);

        if (m_proxyConfiguration.autoConfigUrl.empty())
        {
            DoTraceMessage(WppWarning, "No PAC URL configured for '%!str!', using DIRECT.", url);
            co_return ProxyResolverResult{};
        }

        co_return co_await GetProxyFromOptions(WINHTTP_AUTOPROXY_CONFIG_URL, url, m_proxyConfiguration.autoConfigUrl);
    }

    asio::awaitable<ProxyResolverResult> ProxyResolver::GetProxyFromAutoDetect(const std::string& url) const
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: url='%!str!'", url);

        co_return co_await GetProxyFromOptions(WINHTTP_AUTOPROXY_AUTO_DETECT, url, {});
    }

    asio::awaitable<ProxyResolverResult> ProxyResolver::GetProxyFromOptions(DWORD dwFlags, const std::string& url, const std::string& autoConfigUrl) const
    {
        ProxyResolverResult result = co_await GetProxyFromOptions(dwFlags, url, autoConfigUrl, false);
        if (result.error && result.error.value() == ERROR_WINHTTP_LOGIN_FAILURE)
        {
            DoTraceMessage(WppWarning, "WinHttpGetProxyForUrlEx failed with ERROR_WINHTTP_LOGIN_FAILURE, retrying with autoLogonIfChallenged=TRUE.");
            result = co_await GetProxyFromOptions(dwFlags, url, autoConfigUrl, true);
        }

        co_return result;
    }

    asio::awaitable<ProxyResolverResult> ProxyResolver::GetProxyFromOptions(DWORD dwFlags, const std::string& url, const std::string& autoConfigUrl, bool autoLogonIfChallenged) const
    {
        auto request = std::make_unique<AsyncProxyRequest>();
        request->url = Unicode::FromUtf8(url);
        request->autoConfigUrl = Unicode::FromUtf8(autoConfigUrl);
        request->options.dwFlags = dwFlags;
        request->options.fAutoLogonIfChallenged = autoLogonIfChallenged ? TRUE : FALSE;

        if ((dwFlags & WINHTTP_AUTOPROXY_CONFIG_URL) != 0)
        {
            request->options.lpszAutoConfigUrl = request->autoConfigUrl.c_str();
        }

        if ((dwFlags & WINHTTP_AUTOPROXY_AUTO_DETECT) != 0)
        {
            request->options.dwAutoDetectFlags = WINHTTP_AUTO_DETECT_TYPE_DHCP | WINHTTP_AUTO_DETECT_TYPE_DNS_A;
        }

        co_return co_await asio::async_initiate<decltype(asio::use_awaitable), void(ProxyResolverResult)>(
            [this, request = std::move(request)](auto handler) mutable {
                auto sharedHandler = std::make_shared<decltype(handler)>(std::move(handler));
                request->complete = [sharedHandler](ProxyResolverResult result) {
                    auto executor = asio::get_associated_executor(*sharedHandler);
                    asio::post(executor, [sharedHandler, result = std::move(result)]() mutable {
                        std::move (*sharedHandler)(std::move(result));
                    });
                };

                DWORD dwError = WinHttpCreateProxyResolver(m_session, &request->resolver);
                if (dwError != ERROR_SUCCESS)
                {
                    DoTraceMessage(WppError, "WinHttpCreateProxyResolver failed: %!WINERROR!", dwError);
                    request->complete({ std::error_code(dwError, std::system_category()) });
                    return;
                }

                dwError = WinHttpGetProxyForUrlEx(
                    request->resolver,
                    request->url.c_str(),
                    &request->options,
                    reinterpret_cast<DWORD_PTR>(request.get()));
                if (dwError == ERROR_IO_PENDING)
                {
                    request.release();
                }
                else if (dwError == ERROR_SUCCESS)
                {
                    request->complete(GetResultFromResolver(*request));
                }
                else
                {
                    DoTraceMessage(WppError, "WinHttpGetProxyForUrlEx failed: %!WINERROR!", dwError);
                    request->complete({ std::error_code(dwError, std::system_category()) });
                }
            },
            asio::use_awaitable);
    }

    void CALLBACK ProxyResolver::OnAsyncStatusCallback(HINTERNET hInternet, DWORD_PTR dwContext, DWORD dwInternetStatus, LPVOID lpvStatusInformation, DWORD dwStatusInformationLength)
    {
        UNREFERENCED_PARAMETER(hInternet);
        UNREFERENCED_PARAMETER(dwStatusInformationLength);

        if (dwContext == 0)
        {
            return;
        }

        if (dwInternetStatus == WINHTTP_CALLBACK_STATUS_GETPROXYFORURL_COMPLETE)
        {
            std::unique_ptr<AsyncProxyRequest> request(reinterpret_cast<AsyncProxyRequest*>(dwContext));
            request->complete(GetResultFromResolver(*request));
        }
        else if (dwInternetStatus == WINHTTP_CALLBACK_STATUS_REQUEST_ERROR)
        {
            std::unique_ptr<AsyncProxyRequest> request(reinterpret_cast<AsyncProxyRequest*>(dwContext));
            auto asyncResult = static_cast<const WINHTTP_ASYNC_RESULT*>(lpvStatusInformation);
            DWORD dwError = asyncResult != nullptr ? asyncResult->dwError : ERROR_WINHTTP_INTERNAL_ERROR;
            DoTraceMessage(WppError, "WinHttpGetProxyForUrlEx failed: %!WINERROR!", dwError);
            request->complete({});
        }
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
            WINHTTP_FLAG_ASYNC);
        if (!m_session)
        {
            DWORD dwError = GetLastError();
            DoTraceMessage(WppError, "WinHttpOpen failed: %!WINERROR!", dwError);
            throw Win32Error(dwError, "Failed to open asynchronous WinHTTP session.");
        }

        if (WinHttpSetStatusCallback(
                m_session,
                &ProxyResolver::OnAsyncStatusCallback,
                WINHTTP_CALLBACK_FLAG_REQUEST_ERROR | WINHTTP_CALLBACK_FLAG_GETPROXYFORURL_COMPLETE,
                0) == WINHTTP_INVALID_STATUS_CALLBACK)
        {
            DWORD dwError = GetLastError();
            DoTraceMessage(WppError, "WinHttpSetStatusCallback failed: %!WINERROR!", dwError);
            throw Win32Error(dwError, "Failed to set WinHTTP status callback.");
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

    std::shared_ptr<SystemProxyConfiguration> ProxyResolver::LoadSystemProxyConfiguration()
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

        return systemProxyConfiguration;
    }

    void ProxyResolver::DumpEffectiveProxyConfiguration()
    {
        DoTraceMessage(WppVerbose, "%!FUNC!");

        auto systemProxyConfiguration = m_systemProxyConfiguration.load();

        switch (m_proxyConfiguration.proxyMode)
        {
        case ProxyMode::Default:
        case ProxyMode::System:
            if (systemProxyConfiguration)
            {
                if (!systemProxyConfiguration->autoConfigUrl.empty())
                {
                    DoTraceMessage(WppVerbose, "Proxy mode set to 'System', using PAC URL '%!str!'.", systemProxyConfiguration->autoConfigUrl);
                    LOG_INFO("Using proxy auto-configuration (PAC) URL: {}", systemProxyConfiguration->autoConfigUrl);
                }
                else if (!systemProxyConfiguration->proxy.empty())
                {
                    DoTraceMessage(WppVerbose, "Proxy mode set to 'System', using proxy URL '%!str!'.", systemProxyConfiguration->proxy);
                    LOG_INFO("Using manual proxy configuration: {}", systemProxyConfiguration->proxy);
                }
                else if (systemProxyConfiguration->autoDetect)
                {
                    DoTraceMessage(WppVerbose, "Proxy mode set to 'System', using WPAD.");
                    LOG_INFO("Using proxy auto-detect (WPAD)");
                }
                else
                {
                    DoTraceMessage(WppVerbose, "Proxy mode set to 'System', using 'DIRECT'.");
                    LOG_INFO("Using direct connection (no proxy)");
                }
            }
            else
            {
                DoTraceMessage(WppVerbose, "No system proxy configuration available.");
                LOG_INFO("No system proxy configuration available.");
            }
            break;
        case ProxyMode::Direct:
            DoTraceMessage(WppVerbose, "Proxy mode set to 'Direct', using DIRECT.");
            LOG_INFO("Using direct connection (no proxy)");
            break;
        case ProxyMode::Manual:
            DoTraceMessage(WppVerbose, "Proxy mode set to 'Manual', using proxy URL HTTP='%!str!' and HTTPS='%!str!'.",
                m_proxyConfiguration.httpProxyUrl,
                m_proxyConfiguration.httpsProxyUrl);
            LOG_INFO("Using manual proxy configuration: HTTP='{}', HTTPS='{}'", m_proxyConfiguration.httpProxyUrl, m_proxyConfiguration.httpsProxyUrl);
            break;
        case ProxyMode::AutoConfig:
            DoTraceMessage(WppVerbose, "Proxy mode set to 'AutoConfig', using PAC URL '%!str!'.", m_proxyConfiguration.autoConfigUrl);
            LOG_INFO("Using proxy auto-configuration (PAC) URL: {}", m_proxyConfiguration.autoConfigUrl);
            break;
        case ProxyMode::AutoDetect:
            DoTraceMessage(WppVerbose, "Proxy mode set to 'AutoDetect', using WPAD.");
            LOG_INFO("Using proxy auto-detect (WPAD)");
            break;
        default:
            throw std::invalid_argument("Invalid proxy mode.");
        }
    }

    bool ProxyResolver::IsEqual(std::shared_ptr<SystemProxyConfiguration> old, const std::shared_ptr<SystemProxyConfiguration> current)
    {
        if (old == nullptr && current == nullptr)
        {
            return true;
        }
        else if (old == nullptr || current == nullptr)
        {
            return false;
        }
        else
        {
            return old->autoDetect == current->autoDetect &&
                   old->autoConfigUrl == current->autoConfigUrl &&
                   old->proxy == current->proxy &&
                   old->proxyBypass == current->proxyBypass;
        }
    }
}
