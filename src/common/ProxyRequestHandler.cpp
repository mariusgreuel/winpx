//
// ProxyRequestHandler.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include "pch.h"

#include "ProxyRequestHandler.h"
#include "ProxyRequestHandler.tmh"

#include "HttpError.h"
#include "HttpHelper.h"
#include "HttpTransport.h"
#include "ProxyAuthenticationSession.h"
#include "StringHelper.h"
#include "Uri.h"
#include "WebServer.h"
#include <win32/Base64.h>
#include <win32/Logger.h>

template<>
struct std::formatter<winpx::HttpStatusCode> : std::formatter<std::string_view>
{
    auto format(const winpx::HttpStatusCode& statusCode, std::format_context& ctx) const
    {
        return std::formatter<std::string_view>::format(std::to_string(static_cast<int>(statusCode)), ctx);
    }
};

template<>
struct std::formatter<winpx::HttpMessageFraming> : std::formatter<std::string_view>
{
    auto format(const winpx::HttpMessageFraming& framing, std::format_context& ctx) const
    {
        if (framing.chunked)
        {
            return std::formatter<std::string_view>::format("chunked", ctx);
        }
        else if (framing.contentLength)
        {
            return std::formatter<std::string_view>::format(std::format("{} bytes", *framing.contentLength), ctx);
        }
        else
        {
            return std::formatter<std::string_view>::format("unknown size", ctx);
        }
    }
};

template<>
struct std::formatter<winpx::ProxyRequestHandler::RequestContext> : std::formatter<std::string_view>
{
    auto format(const winpx::ProxyRequestHandler::RequestContext& context, std::format_context& ctx) const
    {
        return std::formatter<std::string_view>::format(std::format("[{}]", context.m_requestId), ctx);
    }
};

namespace winpx
{
    using namespace std::literals::string_view_literals;
    using namespace win32;

    static constexpr size_t maxProxyAuthenticationRounds = 5;
    static constexpr size_t maxReconnects = 2;

    ProxyRequestHandler::ProxyRequestHandler(const ProxyConfiguration& proxyConfiguration, ProxyResolver& proxyResolver, ProxyStatistics& proxyStatistics) :
        HttpTransport(proxyConfiguration.httpConfiguration),
        m_proxyConfiguration(proxyConfiguration),
        m_proxyResolver(proxyResolver),
        m_proxyStatistics(proxyStatistics),
        m_connectionPool(proxyConfiguration.httpConfiguration)
    {
        DoTraceMessage(WppObject, "%!FUNC!");

        m_proxyId = std::format("winpx:{}", proxyConfiguration.port);
    }

    ProxyRequestHandler::~ProxyRequestHandler()
    {
        DoTraceMessage(WppObject, "%!FUNC!");
    }

    asio::awaitable<void> ProxyRequestHandler::Handle(asio::ip::tcp::socket client)
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: [%u/%u]", client.local_endpoint().port(), client.remote_endpoint().port());

        SetTcpNoDelay(client);

        m_proxyStatistics.activeConnections += 1;
        m_proxyStatistics.totalConnections += 1;

        RequestContext context(client);
        std::optional<HttpError> error;

        try
        {
            while (true)
            {
                auto timeout = context.firstRequest ? m_proxyConfiguration.httpConfiguration.readTimeout : m_proxyConfiguration.httpConfiguration.keepAliveTimeout;
                auto [ec, headSize] = co_await asio::async_read_until(
                    client,
                    asio::dynamic_buffer(context.clientBuffer, m_proxyConfiguration.httpConfiguration.maxHeadSize),
                    "\r\n\r\n"sv,
                    asio::as_tuple(asio::cancel_after(timeout, asio::use_awaitable)));
                if (IsConnectionLost(ec) && !context.firstRequest && context.clientBuffer.empty())
                {
                    DoTraceMessage(WppVerbose, "Connection lost");
                    break;
                }
                else if (ec == asio::error::not_found)
                {
                    throw HttpError(ec.message().c_str(), HttpStatusCode::RequestHeaderFieldsTooLarge);
                }
                else if (ec == asio::error::operation_aborted || ec == asio::error::timed_out)
                {
                    throw HttpError(ec.message().c_str(), HttpStatusCode::RequestTimeout);
                }
                else if (ec)
                {
                    throw asio::system_error(ec);
                }

                if (headSize == 0)
                {
                    DoTraceMessage(WppVerbose, "Received empty header");
                    break;
                }

                m_proxyStatistics.totalClientBytesReceived += headSize;

                context.Reuse(++m_proxyStatistics.requests);
                context.request = ParseRequestHead(context.clientBuffer, headSize);
                context.firstRequest = false;

                co_await HandleRequest(context);

                if (!context.keepClientAlive)
                {
                    break;
                }
            }
        }
        catch (const HttpError& e)
        {
            DoTraceMessage(WppWarning, "HTTP error: %s", e.what());
            LOG_WARNING("{} {}", context, e.what());
            error = e;
            m_proxyStatistics.errors += 1;
        }
        catch (const std::exception& e)
        {
            DoTraceMessage(WppWarning, "std::exception: %s", e.what());
            LOG_WARNING("{} {}", context, e.what());
            error = HttpError(HttpStatusCode::BadGateway);
            m_proxyStatistics.errors += 1;
        }

        if (error && !context.responseSent && client.is_open())
        {
            try
            {
                const HttpResponseMessage response(error->GetStatusCode(), error->GetReasonPhrase(),
                    {
                        { "Content-Length"sv, "0"sv },
                        { "Connection"sv, "close"sv },
                    });
                LOG_INFO("{} Writing response: {}", context, response.GetStatusLine());
                m_proxyStatistics.totalClientBytesSent += co_await WriteData(client, response.ToString());
            }
            catch (const std::exception& e)
            {
                DoTraceMessage(WppWarning, "Failed to write error response: %s", e.what());
            }
        }

        LOG_INFO("{} Closing connection", context);
        co_await DrainAndClose(client);

        m_proxyStatistics.activeConnections -= 1;
    }

    asio::awaitable<void> ProxyRequestHandler::HandleRequest(RequestContext& context)
    {
        LOG_INFO("{} Received request: {}", context, context.request.GetRequestLine());

        if (IsRequestToWebServer(context.request))
        {
            co_await HandleWebServerRequest(context);
        }
        else
        {
            co_await HandleProxyRequest(context);
        }
    }

    asio::awaitable<void> ProxyRequestHandler::HandleWebServerRequest(RequestContext& context)
    {
        if (m_proxyConfiguration.requireLocalAuthentication && !IsAuthenticated(context.request, "Authorization"sv))
        {
            DoTraceMessage(WppWarning, "Unauthorized request: '%!str!'", context.request.GetRequestLine());
            LOG_WARNING("{} Unauthorized request: {}", context, context.request.GetRequestLine());

            static const HttpResponseMessage response(HttpStatusCode::Unauthorized, std::nullopt,
                {
                    { "Content-Length"sv, "0"sv },
                    { "WWW-Authenticate"sv, "Basic realm=\"WinPX Proxy\""sv },
                    { "Connection"sv, "close"sv },
                });
            LOG_INFO("{} Writing response: {}", context, response.GetStatusLine());
            context.responseSent = true;
            m_proxyStatistics.totalClientBytesSent += co_await WriteData(context.client, response.ToString());
        }
        else
        {
            WebServer webServer(m_proxyConfiguration, m_proxyStatistics);
            auto response = webServer.ProcessRequest(context.request);
            LOG_INFO("{} Writing response from web-server: {}", context, response.GetStatusLine());
            context.responseSent = true;
            m_proxyStatistics.totalClientBytesSent += co_await WriteData(context.client, response.ToString());
        }
    }

    asio::awaitable<void> ProxyRequestHandler::HandleProxyRequest(RequestContext& context)
    {
        if (m_proxyConfiguration.requireLocalAuthentication && !IsAuthenticated(context.request, "Proxy-Authorization"sv))
        {
            DoTraceMessage(WppWarning, "Unauthorized request: '%!str!'", context.request.GetRequestLine());
            LOG_WARNING("{} Unauthorized request: {}", context, context.request.GetRequestLine());

            static const HttpResponseMessage response(HttpStatusCode::ProxyAuthenticationRequired, std::nullopt,
                {
                    { "Content-Length"sv, "0"sv },
                    { "Proxy-Authenticate"sv, "Basic realm=\"WinPX Proxy\""sv },
                    { "Connection"sv, "close"sv },
                });
            LOG_INFO("{} Writing response: {}", context, response.GetStatusLine());
            context.responseSent = true;
            m_proxyStatistics.totalClientBytesSent += co_await WriteData(context.client, response.ToString());
        }
        else if (IsSentViaThisProxy(context.request))
        {
            DoTraceMessage(WppWarning, "Proxy loop detected: '%!str!'", context.request.GetRequestLine());
            LOG_WARNING("{} Proxy loop detected: {}", context, context.request.GetRequestLine());

            static const HttpResponseMessage response(HttpStatusCode::BadGateway, "Proxy loop detected",
                {
                    { "Content-Length"sv, "0"sv },
                    { "Connection"sv, "close"sv },
                });
            LOG_INFO("{} Writing response: {}", context, response.GetStatusLine());
            context.responseSent = true;
            m_proxyStatistics.totalClientBytesSent += co_await WriteData(context.client, response.ToString());
        }
        else
        {
            if (IEquals(context.request.GetMethod(), "CONNECT"sv))
            {
                co_await HandleConnectRequest(context);
            }
            else
            {
                co_await HandleStandardRequest(context);
            }
        }
    }

    asio::awaitable<void> ProxyRequestHandler::HandleConnectRequest(RequestContext& context)
    {
        DoTraceMessage(WppTrace, "%!FUNC!: [%u/%u], head=%!str!",
            context.client.local_endpoint().port(),
            context.client.remote_endpoint().port(),
            context.request.GetRedactedHead());

        Uri target = context.request.GetRequestUri();
        if (target.port == 0)
        {
            target.port = 443;
        }

        ProxyResolverResult proxy = co_await m_proxyResolver.ResolveProxyAsync(std::format("https://{}/", target.GetHostAndPort()));
        if (proxy.direct)
        {
            DoTraceMessage(WppVerbose, "CONNECT tunnel via DIRECT connection to '%!str!'", target.ToString());

            LOG_INFO("{} Creating direct connection to '{}'", context, target.GetHostAndPort());
            asio::ip::tcp::socket server = co_await ConnectTo(target.host, target.port);

            static const HttpResponseMessage connectionEstablished(HttpStatusCode::OK, "Connection Established");
            LOG_INFO("{} Writing response: {}", context, connectionEstablished.GetStatusLine());
            context.responseSent = true;
            m_proxyStatistics.totalClientBytesSent += co_await WriteData(context.client, connectionEstablished.ToString());

            LOG_INFO("{} Creating tunnel", context);
            auto result = co_await Tunnel(context.client, server, context.clientBuffer, {});
            m_proxyStatistics.totalClientBytesSent += result.clientBytesSent;
            m_proxyStatistics.totalClientBytesReceived += result.clientBytesReceived;
            m_proxyStatistics.totalUpstreamBytesSent += result.serverBytesSent;
            m_proxyStatistics.totalUpstreamBytesReceived += result.serverBytesReceived;
        }
        else
        {
            DoTraceMessage(WppVerbose, "CONNECT tunnel via upstream proxy '%!str!:%d' to '%!str!'", proxy.host, proxy.port, target.ToString());

            HttpRequestMessage connectRequest("CONNECT", target.GetHostAndPort());
            connectRequest.GetHeaders().Add("Host"sv, target.GetHostAndPort());

            auto [server, response] = co_await ConnectToUpstreamProxy(context, proxy.host, proxy.port, std::move(connectRequest));
            if (GetHttpStatusCategory(response.GetStatusCode()) == HttpStatusCategory::Success)
            {
                static const HttpResponseMessage connectionEstablished(HttpStatusCode::OK, "Connection Established");
                LOG_INFO("{} Writing response: {}", context, connectionEstablished.GetStatusLine());
                context.responseSent = true;
                m_proxyStatistics.totalClientBytesSent += co_await WriteData(context.client, connectionEstablished.ToString());

                LOG_INFO("{} Creating tunnel", context);
                auto result = co_await Tunnel(context.client, server, context.clientBuffer, context.serverBuffer);
                m_proxyStatistics.totalClientBytesSent += result.clientBytesSent;
                m_proxyStatistics.totalClientBytesReceived += result.clientBytesReceived;
                m_proxyStatistics.totalUpstreamBytesSent += result.serverBytesSent;
                m_proxyStatistics.totalUpstreamBytesReceived += result.serverBytesReceived;
            }
            else
            {
                auto responseFraming = GetFraming(response);

                LOG_INFO("{} Writing response: {}", context, response.GetStatusLine());
                context.responseSent = true;
                m_proxyStatistics.totalClientBytesSent += co_await WriteData(context.client, response.GetHead());

                LOG_INFO("{} Streaming response body: {}", context, responseFraming);
                size_t totalBytes = co_await StreamResponseBody(server, context.client, context.serverBuffer, responseFraming);
                m_proxyStatistics.totalUpstreamBytesReceived += totalBytes;
                m_proxyStatistics.totalClientBytesSent += totalBytes;
            }
        }
    }

    asio::awaitable<void> ProxyRequestHandler::HandleStandardRequest(RequestContext& context)
    {
        DoTraceMessage(WppTrace, "%!FUNC!: [%u/%u], head=%!str!",
            context.client.local_endpoint().port(),
            context.client.remote_endpoint().port(),
            context.request.GetRedactedHead());

        Uri target = GetAbsoluteUrl(context.request);

        context.requestFraming = GetFraming(context.request);

        HttpRequestMessage forwardedRequest(context.request.GetMethod(), {});
        forwardedRequest.GetHeaders().Add(GetSanitizedHeaders(context.request, context.requestFraming));
        forwardedRequest.GetHeaders().Replace("Host"sv, target.GetHostAndPort());
        forwardedRequest.GetHeaders().Replace("Via"sv, CreateViaThisProxy(context.request));

        ProxyResolverResult proxy = co_await m_proxyResolver.ResolveProxyAsync(target.ToString());
        context.requestStrategy = SelectRequestStrategy(context.request, context.requestFraming, proxy.direct);

        if (proxy.direct)
        {
            DoTraceMessage(WppVerbose, "Forwarding via DIRECT connection to '%!str!'", target.ToString());

            forwardedRequest.SetRequestUri(target.GetPathAndQuery());
            forwardedRequest.GetHeaders().Add("Connection"sv, "keep-alive"sv);

            auto connectionKey = HttpConnectionPool::MakeKey(true, target.host, target.port);

            asio::ip::tcp::socket server(co_await asio::this_coro::executor);
            std::optional<HttpResponseMessage> response;

            std::optional<asio::ip::tcp::socket> pooledConnection = m_connectionPool.Acquire(connectionKey);
            if (pooledConnection)
            {
                LOG_INFO("{} Reusing direct connection to '{}'", context, target.GetHostAndPort());
                server = std::move(*pooledConnection);
                try
                {
                    response = co_await SendRequest(context, server, forwardedRequest);
                }
                catch (const std::exception& e)
                {
                    if (context.requestBodySent)
                    {
                        throw;
                    }

                    DoTraceMessage(WppVerbose, "Pooled connection failed, reconnecting: %s", e.what());
                }
            }

            if (!response)
            {
                LOG_INFO("{} Creating direct connection to '{}'", context, target.GetHostAndPort());
                server = co_await ConnectTo(target.host, target.port);
                response = co_await SendRequest(context, server, forwardedRequest);
            }

            co_await ForwardResponse(context, server, *response, connectionKey);
        }
        else
        {
            DoTraceMessage(WppVerbose, "Forwarding via upstream proxy '%!str!:%d' to '%!str!'", proxy.host, proxy.port, target.ToString());

            forwardedRequest.SetRequestUri(target.ToString());
            forwardedRequest.GetHeaders().Add("Connection"sv, "keep-alive"sv);

            auto connectionKey = HttpConnectionPool::MakeKey(false, proxy.host, proxy.port);

            auto [server, response] = co_await ConnectToUpstreamProxy(context, proxy.host, proxy.port, std::move(forwardedRequest));
            co_await ForwardResponse(context, server, response, connectionKey);
        }
    }

    asio::awaitable<ProxyRequestHandler::ConnectToUpstreamProxyResult> ProxyRequestHandler::ConnectToUpstreamProxy(
        RequestContext& context, const std::string& host, uint16_t port, HttpRequestMessage request)
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: [%u/%u], host=%!str!, port=%u",
            context.client.local_endpoint().port(),
            context.client.remote_endpoint().port(),
            host,
            port);

        asio::ip::tcp::socket server(co_await asio::this_coro::executor);

        std::optional<asio::ip::tcp::socket> pooledConnection = m_connectionPool.Acquire(HttpConnectionPool::MakeKey(false, host, port));
        if (pooledConnection)
        {
            LOG_INFO("{} Reusing connection to upstream proxy '{}:{}'", context, host, port);
            server = std::move(*pooledConnection);
        }
        else
        {
            LOG_INFO("{} Creating connection to upstream proxy '{}:{}'", context, host, port);
            server = co_await ConnectTo(host, port);
        }

        std::unique_ptr<ProxyAuthenticationSession> authenticationSession;
        AuthenticationScheme authenticationScheme = AuthenticationScheme::Any;
        bool connectionBound = false;
        size_t reconnects = 0;

        for (size_t round = 0; round < maxProxyAuthenticationRounds; round++)
        {
            asio::error_code ec;
            bool failed = false;
            HttpResponseMessage response;

            try
            {
                response = co_await SendRequest(context, server, request);
            }
            catch (const asio::system_error& e)
            {
                ec = e.code();
                failed = true;
            }
            catch (const std::exception& e)
            {
                DoTraceMessage(WppWarning, "Invalid server response: %s", e.what());
                failed = true;
            }

            if (failed)
            {
                DoTraceMessage(WppWarning, "Failed to parse server response: %!str!", ec.message());

                if (!connectionBound && !context.requestBodySent && reconnects++ < maxReconnects)
                {
                    context.serverBuffer.clear();
                    server = co_await ConnectTo(host, port);
                    continue;
                }

                throw HttpError(ec.message().c_str(), ec == asio::error::operation_aborted ? HttpStatusCode::GatewayTimeout : HttpStatusCode::BadGateway);
            }

            if (response.GetStatusCode() != HttpStatusCode::ProxyAuthenticationRequired)
            {
                LOG_INFO("{} Upstream proxy response: {}", context, response.GetStatusLine());
                co_return ConnectToUpstreamProxyResult(std::move(server), std::move(response));
            }

            LOG_INFO("{} Upstream proxy '{}:{}' requires authentication", context, host, port);

            if (!IEquals(request.GetMethod(), "HEAD"sv))
            {
                response.SetBody(co_await ReadBody(server, context.serverBuffer, GetFraming(response)));
            }

            if (context.requestBodySent)
            {
                if (!context.replayBuffer || context.replayBuffer->IsOverflow())
                {
                    DoTraceMessage(WppWarning, "Proxy authentication requested after the request body was sent.");
                    throw HttpError("Request body cannot be replayed", HttpStatusCode::BadGateway);
                }

                context.requestStrategy = RequestStrategy::ReplayBody;
            }

            if (IsConnectionClose(response))
            {
                DoTraceMessage(WppVerbose, "Upstream proxy closed the connection after 407, reconnecting.");
                context.serverBuffer.clear();
                server = co_await ConnectTo(host, port);

                if (connectionBound)
                {
                    DoTraceMessage(WppWarning, "Connection-bound authentication interrupted, restarting handshake.");
                    authenticationSession.reset();
                    authenticationScheme = AuthenticationScheme::Any;
                    connectionBound = false;
                    request.GetHeaders() = request.GetHeaders().Select([](auto name) { return !IEquals(name, "Proxy-Authorization"sv); });
                    continue;
                }
            }

            if (authenticationScheme == AuthenticationScheme::Any)
            {
                AuthenticationSchemes offered = ParseAuthenticationOffer(response);
                authenticationScheme = offered.Mask(m_proxyConfiguration.allowedAuthenticationSchemes).PickBest();
                DoTraceMessage(WppVerbose, "Proxy authentication scheme: %!AuthenticationScheme! (offered: %!AuthenticationSchemeFlags!, allowed: %!AuthenticationSchemeFlags!)",
                    static_cast<int>(authenticationScheme),
                    offered.ToValue(),
                    m_proxyConfiguration.allowedAuthenticationSchemes.ToValue());
            }

            if (authenticationScheme == AuthenticationScheme::None)
            {
                DoTraceMessage(WppWarning, "Cannot authenticate at upstream proxy: No matching authentication scheme.");
                throw HttpError(HttpStatusCode::BadGateway, "Upstream proxy authentication failed");
            }
            else if (authenticationScheme == AuthenticationScheme::Basic)
            {
                if (round > 0)
                {
                    DoTraceMessage(WppWarning, "Basic proxy authentication failed.");
                    throw HttpError(HttpStatusCode::BadGateway, "Upstream proxy authentication failed");
                }
                else if (!HasBasicCredentials(m_proxyConfiguration))
                {
                    DoTraceMessage(WppWarning, "Missing basic authentication credentials.");
                    throw HttpError(HttpStatusCode::BadGateway, "Upstream proxy authentication failed");
                }
                else
                {
                    std::string credentials = m_proxyConfiguration.gatewayUsername + ":" + m_proxyConfiguration.gatewayPassword;
                    request.GetHeaders().Replace("Proxy-Authorization"sv, "Basic " + Base64::Encode(credentials));
                    connectionBound = false;
                }
            }
            else
            {
                if (!authenticationSession)
                {
                    authenticationSession = std::make_unique<ProxyAuthenticationSession>(m_proxyConfiguration, authenticationScheme, "HTTP/" + host);
                }

                std::vector<std::byte> incoming = ParseAuthenticationToken(response, ToString(authenticationScheme));
                if (!incoming.empty())
                    connectionBound = true;

                std::vector<std::byte> outgoing;
                SECURITY_STATUS status = authenticationSession->GetNextToken(incoming, outgoing);
                if ((status != SEC_E_OK && status != SEC_I_CONTINUE_NEEDED) || outgoing.empty())
                {
                    DoTraceMessage(WppWarning, "Authentication handshake failed: %!WINERROR!", status);
                    throw HttpError(std::error_code(status, std::system_category()).message().c_str(), HttpStatusCode::BadGateway, "Upstream proxy authentication failed");
                }

                request.GetHeaders().Replace("Proxy-Authorization"sv, ToString(authenticationScheme) + " " + Base64::Encode(outgoing));
            }
        }

        throw HttpError(HttpStatusCode::BadGateway);
    }

    asio::awaitable<HttpResponseMessage> ProxyRequestHandler::SendRequest(RequestContext& context, asio::ip::tcp::socket& server, const HttpRequestMessage& request)
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: [%u/%u -> %u/%u], strategy=%!RequestStrategy!, head=%!str!",
            context.client.local_endpoint().port(),
            context.client.remote_endpoint().port(),
            server.local_endpoint().port(),
            server.remote_endpoint().port(),
            static_cast<int>(context.requestStrategy),
            request.GetRedactedHead());

        switch (context.requestStrategy)
        {
        case RequestStrategy::NoBody:
        {
            LOG_INFO("{} Sending request: {}", context, request.GetRequestLine());
            m_proxyStatistics.totalUpstreamBytesSent += co_await WriteData(server, request.GetHead());
            co_return co_await ReceiveFinalResponse(server, context.serverBuffer);
        }
        case RequestStrategy::StreamBody:
        {
            LOG_INFO("{} Sending request: {}", context, request.GetRequestLine());
            m_proxyStatistics.totalUpstreamBytesSent += co_await WriteData(server, request.GetHead());

            LOG_INFO("{} Streaming request body: {}", context, context.requestFraming);
            context.requestBodySent = true;
            size_t totalBytes = co_await StreamRequestBody(context.client, server, context.clientBuffer, context.requestFraming);
            m_proxyStatistics.totalClientBytesReceived += totalBytes;
            m_proxyStatistics.totalUpstreamBytesSent += totalBytes;

            co_return co_await ReceiveFinalResponse(server, context.serverBuffer);
        }
        case RequestStrategy::StreamAndCaptureBody:
        {
            LOG_INFO("{} Sending request: {}", context, request.GetRequestLine());
            m_proxyStatistics.totalUpstreamBytesSent += co_await WriteData(server, request.GetHead());

            context.replayBuffer = std::make_unique<HttpReplayBuffer>();
            LOG_INFO("{} Streaming request body: {}", context, context.requestFraming);
            context.requestBodySent = true;
            size_t totalBytes = co_await StreamRequestBody(context.client, server, context.clientBuffer, context.requestFraming, context.replayBuffer.get());
            m_proxyStatistics.totalClientBytesReceived += totalBytes;
            m_proxyStatistics.totalUpstreamBytesSent += totalBytes;

            co_return co_await ReceiveFinalResponse(server, context.serverBuffer);
        }
        case RequestStrategy::ReplayBody:
        {
            LOG_INFO("{} Sending request: {}", context, request.GetRequestLine());
            m_proxyStatistics.totalUpstreamBytesSent += co_await WriteData(server, request.GetHead());

            LOG_INFO("{} Sending request body: {}", context, context.requestFraming);
            m_proxyStatistics.totalUpstreamBytesSent += co_await WriteData(server, context.replayBuffer->GetData());
            co_return co_await ReceiveFinalResponse(server, context.serverBuffer);
        }
        case RequestStrategy::ClientExpect:
        case RequestStrategy::InsertExpect:
        {
            std::string head = request.GetHead();
            if (context.requestStrategy == RequestStrategy::InsertExpect)
            {
                head.insert(head.size() - 2, "Expect: 100-continue\r\n"sv);
            }

            LOG_INFO("{} Sending request, expecting 100-continue: {}", context, request.GetRequestLine());
            m_proxyStatistics.totalUpstreamBytesSent += co_await WriteData(server, head);

            auto [finalResponse, timedOut] = co_await WaitForContinue(server, context.serverBuffer);
            if (finalResponse)
            {
                co_return std::move(*finalResponse);
            }

            if (context.requestStrategy == RequestStrategy::ClientExpect && !context.continueSent)
            {
                static const HttpResponseMessage response(HttpStatusCode::Continue);
                LOG_INFO("{} Writing response: {}", context, response.GetStatusLine());
                m_proxyStatistics.totalClientBytesSent += co_await WriteData(context.client, response.ToString());
                context.continueSent = true;
            }

            if (timedOut)
            {
                context.replayBuffer = std::make_unique<HttpReplayBuffer>();
            }

            LOG_INFO("{} Streaming request body: {}", context, context.requestFraming);
            context.requestBodySent = true;
            size_t totalBytes = co_await StreamRequestBody(context.client, server, context.clientBuffer, context.requestFraming, context.replayBuffer.get());
            m_proxyStatistics.totalClientBytesReceived += totalBytes;
            m_proxyStatistics.totalUpstreamBytesSent += totalBytes;

            co_return co_await ReceiveFinalResponse(server, context.serverBuffer);
        }
        default:
            throw HttpError(HttpStatusCode::InternalServerError);
        }
    }

    asio::awaitable<ProxyRequestHandler::WaitForContinueResult> ProxyRequestHandler::WaitForContinue(asio::ip::tcp::socket& server, std::string& buffer) const
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: [%u/%u]", server.local_endpoint().port(), server.remote_endpoint().port());

        while (true)
        {
            auto [ec, headSize] = co_await asio::async_read_until(
                server,
                asio::dynamic_buffer(buffer, m_proxyConfiguration.httpConfiguration.maxHeadSize),
                "\r\n\r\n"sv,
                asio::as_tuple(asio::cancel_after(m_proxyConfiguration.httpConfiguration.expectContinueTimeout, asio::cancellation_type::partial, asio::use_awaitable)));
            if (ec == asio::error::operation_aborted)
            {
                DoTraceMessage(WppWarning, "No response to 'Expect: 100-continue' within timeout, sending body.");
                co_return WaitForContinueResult(std::nullopt, true);
            }
            else if (ec)
            {
                throw asio::system_error(ec);
            }

            auto response = ParseResponseHead(buffer, headSize);

            auto statusCode = response.GetStatusCode();
            DoTraceMessage(WppVerbose, "Received '%!HttpStatusCode!' response within timeout.", static_cast<int>(statusCode));
            if (statusCode == HttpStatusCode::Continue)
            {
                co_return WaitForContinueResult(std::nullopt, false);
            }
            else if (GetHttpStatusCategory(statusCode) == HttpStatusCategory::Informational && statusCode != HttpStatusCode::SwitchingProtocols)
            {
                continue;
            }

            DoTraceMessage(WppVerbose, "response=%!str!", response.GetRedactedHead());
            co_return WaitForContinueResult(std::move(response), false);
        }
    }

    asio::awaitable<HttpResponseMessage> ProxyRequestHandler::ReceiveFinalResponse(asio::ip::tcp::socket& server, std::string& buffer) const
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: [%u/%u]", server.local_endpoint().port(), server.remote_endpoint().port());

        while (true)
        {
            auto headSize = co_await asio::async_read_until(
                server,
                asio::dynamic_buffer(buffer, m_proxyConfiguration.httpConfiguration.maxHeadSize),
                "\r\n\r\n"sv,
                asio::cancel_after(m_proxyConfiguration.httpConfiguration.readTimeout, asio::use_awaitable));

            auto response = ParseResponseHead(buffer, headSize);

            auto statusCode = response.GetStatusCode();
            DoTraceMessage(WppVerbose, "Received '%!HttpStatusCode!'.", static_cast<int>(statusCode));
            if (GetHttpStatusCategory(statusCode) == HttpStatusCategory::Informational && statusCode != HttpStatusCode::SwitchingProtocols)
            {
                continue;
            }

            DoTraceMessage(WppVerbose, "response=%!str!", response.GetRedactedHead());
            co_return response;
        }
    }

    asio::awaitable<void> ProxyRequestHandler::ForwardResponse(RequestContext& context, asio::ip::tcp::socket& server, const HttpResponseMessage& response, const std::string& connectionKey)
    {
        auto responseFraming = GetFraming(response);
        auto statusCode = response.GetStatusCode();
        bool hasBody = !IEquals(context.request.GetMethod(), "HEAD"sv) && statusCode != HttpStatusCode::NoContent && statusCode != HttpStatusCode::NotModified;
        bool isBodySizeKnown = !hasBody || responseFraming.chunked || responseFraming.contentLength.has_value();

        context.keepClientAlive = !IsConnectionClose(context.request) && isBodySizeKnown;

        HttpResponseMessage forwardedResponse(response.GetStatusCode(), response.GetReasonPhrase());
        forwardedResponse.GetHeaders().Add(GetSanitizedHeaders(response, responseFraming));
        forwardedResponse.GetHeaders().Add("Connection"sv, context.keepClientAlive ? "keep-alive"sv : "close"sv);
        LOG_INFO("{} Writing response: {}", context, forwardedResponse.GetStatusLine());
        context.responseSent = true;
        m_proxyStatistics.totalClientBytesSent += co_await WriteData(context.client, forwardedResponse.GetHead());

        if (hasBody)
        {
            LOG_INFO("{} Streaming response body: {}", context, responseFraming);
            size_t totalBytes = co_await StreamResponseBody(server, context.client, context.serverBuffer, responseFraming);
            m_proxyStatistics.totalUpstreamBytesReceived += totalBytes;
            m_proxyStatistics.totalClientBytesSent += totalBytes;
        }

        bool serverReusable = !IsConnectionClose(response) && isBodySizeKnown && statusCode != HttpStatusCode::SwitchingProtocols;
        if (serverReusable && context.serverBuffer.empty())
        {
            m_connectionPool.Release(connectionKey, std::move(server));
        }
    }

    bool ProxyRequestHandler::IsRequestToWebServer(const HttpRequestMessage& request) const
    {
        if (auto host = request.GetHeaders().GetValue("Host"sv))
        {
            return IsRequestToWebServer(Uri(*host));
        }
        else
        {
            return IsRequestToWebServer(request.GetRequestUri());
        }
    }

    bool ProxyRequestHandler::IsRequestToWebServer(const Uri& target) const
    {
        return IsLocalhost(target.host) && target.port == m_proxyConfiguration.port;
    }

    bool ProxyRequestHandler::IsAuthenticated(const HttpRequestMessage& request, std::string_view headerName) const
    {
        if (auto headerValue = request.GetHeaders().GetValue(headerName))
        {
            auto parts = SplitString(*headerValue, ' ');
            if (parts.size() == 2)
            {
                std::string_view scheme = parts[0];
                std::string_view credentials = parts[1];

                if (IEquals(scheme, "Basic"sv))
                {
                    return credentials == Base64::Encode(m_proxyConfiguration.winpxSecret + ":");
                }
            }
        }

        return false;
    }

    bool ProxyRequestHandler::IsSentViaThisProxy(const HttpRequestMessage& request) const
    {
        if (auto via = request.GetHeaders().GetValue("Via"sv))
        {
            return IContains(*via, m_proxyId);
        }

        return false;
    }

    std::string ProxyRequestHandler::CreateViaThisProxy(const HttpRequestMessage& request) const
    {
        if (auto via = request.GetHeaders().GetValue("Via"sv))
        {
            return std::format("{}, {} {}", *via, request.GetVersion() == 10 ? "1.0"sv : "1.1"sv, m_proxyId);
        }
        else
        {
            return std::format("{} {}", request.GetVersion() == 10 ? "1.0"sv : "1.1"sv, m_proxyId);
        }
    }

    ProxyRequestHandler::RequestStrategy ProxyRequestHandler::SelectRequestStrategy(const HttpRequestMessage& request, const HttpMessageFraming& framing, bool direct) const
    {
        if (!framing.chunked && framing.contentLength.value_or(0) == 0)
        {
            // Client request has no body (GET, HEAD, CONNECT, Content-Length: 0).
            // Direct: Send the request head and receive the response.
            // Proxy: Every upstream proxy authentication attempt sends the complete request head.
            return RequestStrategy::NoBody;
        }
        else if (HasExpectContinue(request))
        {
            // Client sent 'Expect: 100-continue'.
            // Direct: Send the request head and receive the response.
            // Proxy: Authenticate on head alone, then send '100 Continue' to client and stream body.
            return RequestStrategy::ClientExpect;
        }
        else if (direct)
        {
            // Direct connection to the target server, no upstream proxy.
            // Stream the body immediately, no need to buffer it for authentication.
            return RequestStrategy::StreamBody;
        }
        else
        {
            if (framing.contentLength && *framing.contentLength <= m_proxyConfiguration.httpConfiguration.maxBodySizeBeforeProbing)
            {
                // Upstream proxy authentication may be required.
                // Stream immediately, buffer the body for replay.
                return RequestStrategy::StreamAndCaptureBody;
            }
            else
            {
                // Upstream proxy authentication may be required, insert 'Expect: 100-continue'.
                // Authenticate on head alone, then stream body.
                return RequestStrategy::InsertExpect;
            }
        }
    }

    HttpRequestMessage ProxyRequestHandler::ParseRequestHead(std::string& buffer, size_t headSize)
    {
        try
        {
            HttpRequestMessage request;
            request.ParseHead(std::string_view(buffer).substr(0, headSize));
            buffer.erase(0, headSize);
            return request;
        }
        catch (const std::exception& e)
        {
            DoTraceMessage(WppWarning, "Invalid request: %s", e.what());
            throw HttpError(e.what(), HttpStatusCode::BadRequest);
        }
    }

    HttpResponseMessage ProxyRequestHandler::ParseResponseHead(std::string& buffer, size_t headSize)
    {
        try
        {
            HttpResponseMessage response;
            response.ParseHead(std::string_view(buffer).substr(0, headSize));
            buffer.erase(0, headSize);
            return response;
        }
        catch (const std::exception& e)
        {
            DoTraceMessage(WppWarning, "Invalid response: %s", e.what());
            throw HttpError(e.what(), HttpStatusCode::BadGateway);
        }
    }

    HttpMessageFraming ProxyRequestHandler::GetFraming(const HttpRequestMessage& request)
    {
        try
        {
            return request.GetFraming();
        }
        catch (const std::exception& e)
        {
            DoTraceMessage(WppWarning, "Invalid request framing: %s", e.what());
            throw HttpError(e.what(), HttpStatusCode::BadRequest);
        }
    }

    HttpMessageFraming ProxyRequestHandler::GetFraming(const HttpResponseMessage& response)
    {
        try
        {
            return response.GetFraming();
        }
        catch (const std::exception& e)
        {
            DoTraceMessage(WppWarning, "Invalid response framing: %s", e.what());
            throw HttpError(e.what(), HttpStatusCode::BadGateway);
        }
    }

    AuthenticationSchemes ProxyRequestHandler::ParseAuthenticationOffer(const HttpResponseMessage& response)
    {
        AuthenticationSchemes offered;

        for (const auto& value : response.GetHeaders().GetValues("Proxy-Authenticate"sv))
        {
            auto [scheme, parameter] = SplitAuthenticationHeader(value);
            if (IEquals(scheme, "Negotiate"sv))
            {
                offered.Add(AuthenticationScheme::Negotiate);
            }
            else if (IEquals(scheme, "NTLM"sv))
            {
                offered.Add(AuthenticationScheme::Ntlm);
            }
            else if (IEquals(scheme, "Digest"sv))
            {
                offered.Add(AuthenticationScheme::Digest);
            }
            else if (IEquals(scheme, "Basic"sv))
            {
                offered.Add(AuthenticationScheme::Basic);
            }
        }

        return offered;
    }

    std::vector<std::byte> ProxyRequestHandler::ParseAuthenticationToken(const HttpResponseMessage& response, std::string_view scheme)
    {
        for (const auto& value : response.GetHeaders().GetValues("Proxy-Authenticate"sv))
        {
            auto [headerScheme, parameter] = SplitAuthenticationHeader(value);
            if (IEquals(headerScheme, scheme) && !parameter.empty())
            {
                return Base64::Decode(parameter);
            }
        }

        return {};
    }

    std::tuple<std::string_view, std::string_view> ProxyRequestHandler::SplitAuthenticationHeader(std::string_view value)
    {
        auto pos = value.find(' ');
        if (pos != std::string_view::npos)
        {
            auto scheme = value.substr(0, pos);
            auto parameter = value.substr(value.find_first_not_of(' ', pos + 1));
            return { scheme, parameter };
        }
        else
        {
            return { value, {} };
        }
    }

    HttpHeaders ProxyRequestHandler::GetSanitizedHeaders(const HttpMessage& message, const HttpMessageFraming& framing)
    {
        auto connectionOptions = message.GetHeaders().GetListValues("Connection"sv);
        auto headers = message.GetHeaders().Select([&](auto name) {
            return !IsHopByHopHeader(name) &&
                   !HasConnectionOption(connectionOptions, name) &&
                   !IEquals(name, "Content-Length"sv);
        });

        if (framing.chunked)
        {
            headers.Replace("Transfer-Encoding"sv, "chunked"sv);
        }
        else if (framing.contentLength)
        {
            headers.Replace("Content-Length"sv, std::to_string(*framing.contentLength));
        }

        return headers;
    }

    bool ProxyRequestHandler::IsHopByHopHeader(std::string_view name)
    {
        return IEquals(name, "Connection"sv) ||
               IEquals(name, "Keep-Alive"sv) ||
               IEquals(name, "Proxy-Authenticate"sv) ||
               IEquals(name, "Proxy-Authorization"sv) ||
               IEquals(name, "Proxy-Connection"sv) ||
               IEquals(name, "TE"sv) ||
               IEquals(name, "Trailer"sv) ||
               IEquals(name, "Transfer-Encoding"sv) ||
               IEquals(name, "Upgrade"sv);
    }

    bool ProxyRequestHandler::IsConnectionClose(const HttpMessage& message)
    {
        auto options = message.GetHeaders().GetListValues("Connection"sv);
        if (HasConnectionOption(options, "close"sv))
        {
            return true;
        }

        return message.GetVersion() == 10 && !HasConnectionOption(options, "keep-alive"sv);
    }

    bool ProxyRequestHandler::HasConnectionOption(const std::vector<std::string_view>& connectionOptions, std::string_view name)
    {
        for (auto value : connectionOptions)
        {
            if (IEquals(value, name))
            {
                return true;
            }
        }

        return false;
    }

    bool ProxyRequestHandler::HasExpectContinue(const HttpMessage& message)
    {
        if (message.GetVersion() == 10)
        {
            return false;
        }

        for (auto value : message.GetHeaders().GetListValues("Expect"sv))
        {
            if (IEquals(value, "100-continue"sv))
            {
                return true;
            }
        }

        return false;
    }

    bool ProxyRequestHandler::HasBasicCredentials(const ProxyConfiguration& configuration)
    {
        return !configuration.gatewayUsername.empty();
    }

    bool ProxyRequestHandler::IsConnectionLost(const asio::error_code& ec)
    {
        return ec == asio::error::eof ||
               ec == asio::error::connection_reset ||
               ec == asio::error::connection_aborted ||
               ec == asio::error::operation_aborted ||
               ec == asio::error::timed_out;
    }

    INCLUDE_IN_PDB(AuthenticationScheme);
    INCLUDE_IN_PDB(AuthenticationSchemeFlags);
}
