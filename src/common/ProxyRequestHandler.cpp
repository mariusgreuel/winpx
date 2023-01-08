//
// ProxyRequestHandler.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include "pch.h"

#include "ProxyRequestHandler.h"
#include "ProxyRequestHandler.tmh"

#include "HttpError.h"
#include "ProxyAuthenticationSession.h"
#include "Tools.h"
#include "Uri.h"
#include "WebServer.h"
#include <win32/Base64.h>

namespace winpx
{
    using namespace std::literals::string_view_literals;
    using namespace win32;

    static constexpr size_t maxHeaderBufferSize = 0x100000;
    static constexpr size_t maxBufferSize = 0x10000;
    static constexpr size_t maxProxyAuthenticationRounds = 5;
    static constexpr size_t maxChunkSize = 0x40000000;

    ProxyRequestHandler::ProxyRequestHandler(const ProxyConfiguration& proxyConfiguration, ProxyResolver& proxyResolver, ProxyStatistics& proxyStatistics) :
        m_proxyConfiguration(proxyConfiguration),
        m_proxyResolver(proxyResolver),
        m_proxyStatistics(proxyStatistics)
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
        DoTraceMessage(WppVerbose, "%!FUNC!");

        m_proxyStatistics.activeConnections += 1;

        std::optional<HttpResponseMessage> errorResponse;

        try
        {
            std::string message;
            auto headSize = co_await asio::async_read_until(
                client,
                asio::dynamic_buffer(message, maxHeaderBufferSize),
                "\r\n\r\n"sv,
                asio::cancel_after(m_proxyConfiguration.readTimeout, asio::use_awaitable));
            if (headSize > 0)
            {
                HttpRequestMessage request;
                request.Parse(message, headSize);

                m_proxyStatistics.totalConnections += 1;
                m_proxyStatistics.totalClientBytesReceived += message.size();

                if (IsRequestToWebServer(request))
                {
                    co_await HandleWebServerRequest(client, request);
                }
                else
                {
                    co_await HandleProxyRequest(client, request);
                }
            }
        }
        catch (const HttpError& e)
        {
            DoTraceMessage(WppWarning, "HTTP error: %s", e.what());
            errorResponse.emplace(e.GetStatusCode(), e.GetReasonPhrase());
            m_proxyStatistics.errors += 1;
        }
        catch (const std::exception& e)
        {
            DoTraceMessage(WppWarning, "std::exception: %s", e.what());
            errorResponse.emplace(HttpStatusCode::BadGateway);
            m_proxyStatistics.errors += 1;
        }

        if (errorResponse && client.is_open())
        {
            try
            {
                m_proxyStatistics.totalClientBytesSent += co_await WriteResponse(client, std::move(*errorResponse));
            }
            catch (const std::exception& e)
            {
                DoTraceMessage(WppWarning, "Failed to write error response: %s", e.what());
            }
        }

        m_proxyStatistics.activeConnections -= 1;

        DoTraceMessage(WppVerbose, "%!FUNC!: Closing connection...");
        asio::error_code ec;
        client.shutdown(asio::socket_base::shutdown_both, ec);
        client.close(ec);
    }

    asio::awaitable<void> ProxyRequestHandler::HandleWebServerRequest(asio::ip::tcp::socket& client, const HttpRequestMessage& request)
    {
        if (m_proxyConfiguration.requireLocalAuthentication && !IsAuthenticated(request, "Authorization"sv))
        {
            DoTraceMessage(WppWarning, "Unauthorized request: '%!str!'", request.GetRequestLine());

            HttpResponseMessage response(HttpStatusCode::Unauthorized);
            response.GetHeaders().Add("WWW-Authenticate", "Basic realm=\"WinPX Proxy\"");
            m_proxyStatistics.totalClientBytesSent += co_await WriteResponse(client, std::move(response));
        }
        else
        {
            WebServer webServer(m_proxyConfiguration, m_proxyStatistics);
            m_proxyStatistics.totalClientBytesSent += co_await WriteResponse(client, webServer.ProcessRequest(request));
        }
    }

    asio::awaitable<void> ProxyRequestHandler::HandleProxyRequest(asio::ip::tcp::socket& client, const HttpRequestMessage& request)
    {
        if (m_proxyConfiguration.requireLocalAuthentication && !IsAuthenticated(request, "Proxy-Authorization"sv))
        {
            DoTraceMessage(WppWarning, "Unauthorized request: '%!str!'", request.GetRequestLine());

            HttpResponseMessage response(HttpStatusCode::ProxyAuthenticationRequired);
            response.GetHeaders().Add("Proxy-Authenticate", "Basic realm=\"WinPX Proxy\"");
            m_proxyStatistics.totalClientBytesSent += co_await WriteResponse(client, std::move(response));
        }
        else if (IsSentViaThisProxy(request))
        {
            m_proxyStatistics.totalClientBytesSent += co_await WriteResponse(client, { HttpStatusCode::BadGateway, "Proxy loop detected" });
        }
        else
        {
            if (IEquals(request.GetMethod(), "CONNECT"sv))
            {
                co_await HandleConnectRequest(client, request);
            }
            else
            {
                co_await HandleStandardRequest(client, request);
            }
        }
    }

    asio::awaitable<void> ProxyRequestHandler::HandleConnectRequest(asio::ip::tcp::socket& client, const HttpRequestMessage& request)
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: request='%!str!'", request.GetRequestLine());

        Uri target(request.GetRequestUri());
        if (target.port == 0)
            target.port = 443;

        auto proxy = m_proxyResolver.ResolveProxy(std::format("https://{}/", target.GetHostAndPort()));
        if (proxy.direct)
        {
            DoTraceMessage(WppProxy, "CONNECT tunnel via DIRECT connection to '%!str!'", target.ToString());

            asio::ip::tcp::socket remote = co_await ConnectTo(target.host, target.port);
            m_proxyStatistics.totalClientBytesSent += co_await WriteResponse(client, { HttpStatusCode::OK, "Connection Established" });
            co_await Tunnel(client, remote, request.GetBody(), {});
        }
        else
        {
            DoTraceMessage(WppProxy, "CONNECT tunnel via upstream proxy '%!str!:%d' to '%!str!'", proxy.host, proxy.port, target.ToString());

            HttpRequestMessage upstreamRequest(std::string("CONNECT"sv), target.GetHostAndPort());
            upstreamRequest.GetHeaders().Add("Host"sv, target.GetHostAndPort());

            auto [upstream, response] = co_await ConnectToUpstreamProxy(proxy, std::move(upstreamRequest), true);
            if (upstream.is_open())
            {
                if (response.GetStatusCode() == HttpStatusCode::OK)
                {
                    m_proxyStatistics.totalClientBytesSent += co_await WriteResponse(client, { HttpStatusCode::OK, "Connection Established" });
                    co_await Tunnel(client, upstream, request.GetBody(), response.GetBody());
                }
                else
                {
                    m_proxyStatistics.totalClientBytesSent += co_await WriteData(client, response.GetHead());
                    co_await ForwardBody(upstream, client, response);
                }
            }
            else
            {
                m_proxyStatistics.totalClientBytesSent += co_await WriteResponse(client, std::move(response));
            }
        }
    }

    asio::awaitable<void> ProxyRequestHandler::HandleStandardRequest(asio::ip::tcp::socket& client, const HttpRequestMessage& request)
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: request='%!str!'", request.GetRequestLine());

        Uri target = GetAbsoluteUrl(request);

        std::string body = co_await ReadBody(client, request);

        HttpRequestMessage forwardedRequest(request.GetMethod(), {});
        forwardedRequest.GetHeaders().Add(FilterHeaders(request.GetHeaders(), [](auto name) { return !IsHopByHopHeader(name); }));
        forwardedRequest.GetHeaders().Replace("Host"sv, target.GetHostAndPort());
        forwardedRequest.GetHeaders().Replace("Via"sv, CreateViaThisProxy(request));
        forwardedRequest.GetHeaders().Replace("Content-Length"sv, std::to_string(body.size()));
        forwardedRequest.SetBody(std::move(body));

        auto proxy = m_proxyResolver.ResolveProxy(target.ToString());
        if (proxy.direct)
        {
            DoTraceMessage(WppProxy, "Forwarding via DIRECT connection to '%!str!'", target.ToString());

            forwardedRequest.SetRequestUri(target.GetPathAndQuery());
            forwardedRequest.GetHeaders().Add("Connection"sv, "close"sv);

            asio::ip::tcp::socket remote = co_await ConnectTo(target.host, target.port);
            co_await WriteData(remote, forwardedRequest.ToString());
            co_await RelayDataOneWay(remote, client);
        }
        else
        {
            DoTraceMessage(WppProxy, "Forwarding via upstream proxy '%!str!:%d' to '%!str!'", proxy.host, proxy.port, target.ToString());

            forwardedRequest.SetRequestUri(target.ToString());
            forwardedRequest.GetHeaders().Add("Connection"sv, "keep-alive"sv);

            bool hasBody = !IEquals(request.GetMethod(), "HEAD"sv);

            auto [upstream, response] = co_await ConnectToUpstreamProxy(proxy, std::move(forwardedRequest), hasBody);
            if (upstream.is_open())
            {
                m_proxyStatistics.totalClientBytesSent += co_await WriteData(client, response.GetHead());

                auto statusCode = response.GetStatusCode();
                if (hasBody && statusCode != HttpStatusCode::NoContent && statusCode != HttpStatusCode::NotModified)
                {
                    co_await ForwardBody(upstream, client, response);
                }
            }
            else
            {
                m_proxyStatistics.totalClientBytesSent += co_await WriteResponse(client, std::move(response));
            }
        }
    }

    asio::awaitable<ProxyRequestHandler::ProxyConnection> ProxyRequestHandler::ConnectToUpstreamProxy(const ProxyResolverResult& proxy, HttpRequestMessage request, bool hasBody)
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: host=%!str!, port=%u", proxy.host, proxy.port);

        asio::ip::tcp::socket upstream = co_await ConnectTo(proxy.host, proxy.port);

        std::unique_ptr<ProxyAuthenticationSession> authenticationSession;
        AuthenticationScheme authenticationScheme = AuthenticationScheme::Any;
        bool connectionBound = false;
        int reconnects = 0;

        for (size_t round = 0; round < maxProxyAuthenticationRounds; round++)
        {
            asio::error_code ec;
            HttpResponseMessage response;

            co_await asio::async_write(
                upstream,
                asio::buffer(request.ToString()),
                asio::redirect_error(asio::cancel_after(m_proxyConfiguration.writeTimeout, asio::use_awaitable), ec));
            if (!ec)
            {
                std::string message;
                auto headSize = co_await asio::async_read_until(
                    upstream, asio::dynamic_buffer(message, maxHeaderBufferSize),
                    "\r\n\r\n"sv,
                    asio::redirect_error(asio::cancel_after(m_proxyConfiguration.readTimeout, asio::use_awaitable), ec));
                if (!ec && headSize > 0)
                {
                    response.Parse(message, headSize);
                }
            }

            if (ec || response.GetStatusCode() == HttpStatusCode::Unknown)
            {
                DoTraceMessage(WppWarning, "Failed to parse upstream proxy response: %!str!", ec.message());

                if (!connectionBound && reconnects++ < 2)
                {
                    upstream = co_await ConnectTo(proxy.host, proxy.port);
                    continue;
                }

                if (ec == asio::error::operation_aborted)
                {
                    throw HttpError(HttpStatusCode::GatewayTimeout);
                }
                else
                {
                    throw HttpError(HttpStatusCode::BadGateway);
                }
            }

            if (response.GetStatusCode() != HttpStatusCode::ProxyAuthenticationRequired)
            {
                co_return ProxyConnection(std::move(upstream), std::move(response));
            }

            if (hasBody)
            {
                response.SetBody(co_await ReadBody(upstream, response));
            }

            if (authenticationScheme == AuthenticationScheme::Any)
            {
                AuthenticationSchemes offered = ParseAuthenticationOffer(response);
                authenticationScheme = offered.Mask(m_proxyConfiguration.allowedAuthenticationSchemes).PickBest();
                DoTraceMessage(WppVerbose, "Proxy authentication scheme: %!PROXY_AUTHENTICATION_SCHEME! (offered: 0x%X, allowed: 0x%X)",
                    static_cast<int>(authenticationScheme),
                    offered.ToValue(),
                    m_proxyConfiguration.allowedAuthenticationSchemes.ToValue());
            }

            if (authenticationScheme == AuthenticationScheme::None)
            {
                DoTraceMessage(WppWarning, "Cannot authenticate at upstream proxy: No matching authentication scheme.");
                upstream.close(ec);
                co_return ProxyConnection(std::move(upstream), std::move(response));
            }
            else if (authenticationScheme == AuthenticationScheme::Basic)
            {
                if (round > 0)
                {
                    DoTraceMessage(WppWarning, "Basic proxy authentication failed.");
                    upstream.close(ec);
                    co_return ProxyConnection(std::move(upstream), std::move(response));
                }
                else if (!HasBasicCredentials(m_proxyConfiguration))
                {
                    DoTraceMessage(WppWarning, "Missing basic authentication credentials.");
                    upstream.close(ec);
                    co_return ProxyConnection(std::move(upstream), std::move(response));
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
                    authenticationSession = std::make_unique<ProxyAuthenticationSession>(m_proxyConfiguration, authenticationScheme, "HTTP/" + proxy.host);
                }

                std::vector<std::byte> incoming = ParseAuthenticationToken(response, ToString(authenticationScheme));
                if (!incoming.empty())
                    connectionBound = true;

                std::vector<std::byte> outgoing;
                SECURITY_STATUS status = authenticationSession->GetNextToken(incoming, outgoing);
                if ((status != SEC_E_OK && status != SEC_I_CONTINUE_NEEDED) || outgoing.empty())
                {
                    DoTraceMessage(WppWarning, "Authentication handshake failed: %!WINERROR!", status);
                    upstream.close(ec);
                    co_return ProxyConnection(std::move(upstream), std::move(response));
                }

                request.GetHeaders().Replace("Proxy-Authorization"sv, ToString(authenticationScheme) + " " + Base64::Encode(outgoing));
            }
        }

        throw HttpError(HttpStatusCode::BadGateway);
    }

    asio::awaitable<asio::ip::tcp::socket> ProxyRequestHandler::ConnectTo(std::string_view host, uint16_t port)
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: host=%!sv!, port=%u", host, port);

        asio::any_io_executor executor = co_await asio::this_coro::executor;
        asio::ip::tcp::socket socket(executor);
        asio::ip::tcp::resolver resolver(executor);
        asio::ip::tcp::resolver::results_type results = co_await resolver.async_resolve(
            host,
            std::to_string(port),
            asio::cancel_after(m_proxyConfiguration.resolveTimeout, asio::use_awaitable));
        co_await asio::async_connect(
            socket,
            results,
            asio::cancel_after(m_proxyConfiguration.connectTimeout, asio::use_awaitable));
        co_return socket;
    }

    asio::awaitable<std::string> ProxyRequestHandler::ReadBody(asio::ip::tcp::socket& client, const HttpMessage& message)
    {
        DoTraceMessage(WppVerbose, "%!FUNC!");

        try
        {
            auto storage = message.GetBody();

            auto contentLength = message.GetHeaders().GetValue("Content-Length"sv);
            auto transferEncoding = message.GetHeaders().GetValue("Transfer-Encoding"sv);
            if (contentLength && transferEncoding)
            {
                throw HttpError(HttpStatusCode::BadRequest, "Both Content-Length and Transfer-Encoding headers are present.");
            }
            else if (contentLength)
            {
                co_return co_await ReadData(client, storage, ParseDec<size_t>(*contentLength));
            }
            else if (transferEncoding)
            {
                if (IEquals(*transferEncoding, "chunked"sv))
                {
                    co_return co_await ReadChunkedData(client, storage);
                }
                else
                {
                    throw HttpError(HttpStatusCode::NotImplemented, "Unsupported transfer encoding.");
                }
            }
            else
            {
                co_return std::string();
            }
        }
        catch (const std::exception& e)
        {
            DoTraceMessage(WppWarning, "Failed to read body: %s", e.what());
            throw HttpError(HttpStatusCode::BadRequest, e.what());
        }
    }

    asio::awaitable<void> ProxyRequestHandler::ForwardBody(asio::ip::tcp::socket& upstream, asio::ip::tcp::socket& client, const HttpMessage& message)
    {
        DoTraceMessage(WppVerbose, "%!FUNC!");

        try
        {
            auto storage = message.GetBody();

            size_t totalBytes = 0;

            auto contentLength = message.GetHeaders().GetValue("Content-Length"sv);
            auto transferEncoding = message.GetHeaders().GetValue("Transfer-Encoding"sv);
            if (contentLength && transferEncoding)
            {
                throw HttpError(HttpStatusCode::BadRequest, "Both Content-Length and Transfer-Encoding headers are present.");
            }
            else if (contentLength)
            {
                totalBytes = co_await ForwardData(upstream, client, storage, ParseDec<size_t>(*contentLength));
            }
            else if (transferEncoding)
            {
                if (IEquals(*transferEncoding, "chunked"sv))
                {
                    totalBytes = co_await ForwardChunkedData(upstream, client, storage);
                }
                else
                {
                    throw HttpError(HttpStatusCode::NotImplemented, "Unsupported transfer encoding.");
                }
            }
            else
            {
                totalBytes = co_await ForwardData(upstream, client, storage, std::numeric_limits<size_t>::max());
            }

            m_proxyStatistics.totalUpstreamBytesReceived += totalBytes;
            m_proxyStatistics.totalClientBytesSent += totalBytes;
        }
        catch (const std::exception& e)
        {
            DoTraceMessage(WppWarning, "Failed to forward body: %s", e.what());
            throw HttpError(HttpStatusCode::BadRequest, e.what());
        }
    }

    asio::awaitable<void> ProxyRequestHandler::Tunnel(asio::ip::tcp::socket& client, asio::ip::tcp::socket& upstream, std::string_view clientLeftover, std::string_view upstreamLeftover)
    {
        DoTraceMessage(WppVerbose, "%!FUNC!");

        size_t clientBytesSent = 0;
        size_t upstreamBytesSent = 0;

        if (!clientLeftover.empty())
        {
            upstreamBytesSent += co_await WriteData(upstream, clientLeftover);
        }

        if (!upstreamLeftover.empty())
        {
            clientBytesSent += co_await WriteData(client, upstreamLeftover);
        }

        auto [clientBytesRelayed, serverBytesRelayed] = co_await RelayData(client, upstream);

        m_proxyStatistics.totalClientBytesSent += clientBytesSent + serverBytesRelayed;
        m_proxyStatistics.totalClientBytesReceived += clientBytesRelayed;
        m_proxyStatistics.totalUpstreamBytesSent += upstreamBytesSent + clientBytesRelayed;
        m_proxyStatistics.totalUpstreamBytesReceived += serverBytesRelayed;
    }


    asio::awaitable<std::tuple<size_t, size_t>> ProxyRequestHandler::RelayData(asio::ip::tcp::socket& client, asio::ip::tcp::socket& upstream) const
    {
        DoTraceMessage(WppVerbose, "%!FUNC!");

        using namespace asio::experimental::awaitable_operators;
        co_return co_await (RelayDataOneWay(client, upstream) && RelayDataOneWay(upstream, client));
    }

    asio::awaitable<size_t> ProxyRequestHandler::RelayDataOneWay(asio::ip::tcp::socket& source, asio::ip::tcp::socket& destination) const
    {
        DoTraceMessage(WppVerbose, "%!FUNC!");

        std::vector<char> buffer(maxBufferSize);

        size_t totalBytesRelayed = 0;

        try
        {
            while (true)
            {
                size_t bytesRead = co_await source.async_read_some(
                    asio::buffer(buffer),
                    asio::cancel_after(m_proxyConfiguration.readTimeout, asio::use_awaitable));
                co_await asio::async_write(
                    destination,
                    asio::buffer(buffer, bytesRead),
                    asio::cancel_after(m_proxyConfiguration.writeTimeout, asio::use_awaitable));
                totalBytesRelayed += bytesRead;
            }
        }
        catch (const std::system_error& e)
        {
            auto& code = e.code();
            if (code != asio::error::eof &&
                code != std::error_code(WSAECONNABORTED, asio::system_category()) &&
                code != std::error_code(WSAECONNRESET, asio::system_category()))
            {
                DoTraceMessage(WppWarning, "Failed to relay data: %s", e.what());
            }
        }

        asio::error_code ec;
        destination.shutdown(asio::socket_base::shutdown_send, ec);

        co_return totalBytesRelayed;
    }

    asio::awaitable<std::string> ProxyRequestHandler::ReadData(asio::ip::tcp::socket& socket, std::string& storage, size_t contentLength) const
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: contentLength=%llu", contentLength);

        if (contentLength == 0)
            co_return std::string();

        std::string result;

        auto buffer = asio::dynamic_buffer(storage, maxBufferSize);

        size_t bytesRemaining = contentLength;
        while (bytesRemaining > 0)
        {
            asio::error_code ec;

            if (buffer.size() == 0)
            {
                size_t bytesToRead = std::min(bytesRemaining, maxBufferSize);
                co_await asio::async_read(
                    socket,
                    buffer,
                    asio::transfer_exactly(bytesToRead),
                    asio::redirect_error(asio::cancel_after(m_proxyConfiguration.readTimeout, asio::use_awaitable), ec));
                if (ec && ec != asio::error::eof)
                {
                    throw asio::system_error(ec);
                }
            }

            size_t bytesToCopy = std::min(buffer.size(), bytesRemaining);
            result += storage.substr(0, bytesToCopy);
            buffer.consume(bytesToCopy);
            bytesRemaining -= bytesToCopy;

            if (ec == asio::error::eof)
            {
                DoTraceMessage(WppWarning, "Failed to read data: %!str!", ec.message());
                break;
            }
        }

        co_return result;
    }

    asio::awaitable<std::string> ProxyRequestHandler::ReadChunkedData(asio::ip::tcp::socket& socket, std::string& storage) const
    {
        DoTraceMessage(WppVerbose, "%!FUNC!");

        std::string result;

        auto buffer = asio::dynamic_buffer(storage, maxBufferSize);

        while (true)
        {
            auto lineSize = co_await asio::async_read_until(
                socket,
                buffer,
                '\n',
                asio::cancel_after(m_proxyConfiguration.readTimeout, asio::use_awaitable));
            if (lineSize == 0)
                break;

            size_t chunkSize = ParseChunkSize(std::string_view(storage.data(), lineSize));

            buffer.consume(lineSize);

            result += co_await ReadData(socket, storage, chunkSize);

            lineSize = co_await asio::async_read_until(
                socket,
                buffer,
                '\n',
                asio::cancel_after(m_proxyConfiguration.readTimeout, asio::use_awaitable));
            if (lineSize == 0)
                break;

            buffer.consume(lineSize);

            if (chunkSize == 0)
            {
                break;
            }
        }

        co_return result;
    }

    asio::awaitable<size_t> ProxyRequestHandler::ForwardData(asio::ip::tcp::socket& source, asio::ip::tcp::socket& destination, std::string& storage, size_t contentLength) const
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: contentLength=%llu", contentLength);

        if (contentLength == 0)
            co_return 0;

        auto buffer = asio::dynamic_buffer(storage, maxBufferSize);

        size_t totalBytes = 0;
        size_t bytesRemaining = contentLength;
        while (bytesRemaining > 0)
        {
            asio::error_code ec;

            if (buffer.size() == 0)
            {
                size_t bytesToRead = std::min(bytesRemaining, maxBufferSize);
                co_await asio::async_read(
                    source,
                    buffer,
                    asio::transfer_exactly(bytesToRead),
                    asio::redirect_error(asio::cancel_after(m_proxyConfiguration.readTimeout, asio::use_awaitable), ec));
                if (ec && ec != asio::error::eof)
                {
                    throw asio::system_error(ec);
                }
            }

            size_t bytesToWrite = std::min(buffer.size(), bytesRemaining);
            size_t bytesWritten = co_await asio::async_write(
                destination,
                buffer.data(0, bytesToWrite),
                asio::cancel_after(m_proxyConfiguration.writeTimeout, asio::use_awaitable));

            buffer.consume(bytesWritten);
            bytesRemaining -= bytesWritten;
            totalBytes += bytesWritten;

            if (ec == asio::error::eof)
            {
                DoTraceMessage(WppWarning, "Failed to read data: %!str!", ec.message());
                break;
            }
        }

        co_return totalBytes;
    }

    asio::awaitable<size_t> ProxyRequestHandler::ForwardChunkedData(asio::ip::tcp::socket& source, asio::ip::tcp::socket& destination, std::string& storage) const
    {
        DoTraceMessage(WppVerbose, "%!FUNC!");

        auto buffer = asio::dynamic_buffer(storage, maxBufferSize);

        size_t totalBytes = 0;
        while (true)
        {
            auto lineSize = co_await asio::async_read_until(
                source,
                buffer,
                '\n',
                asio::cancel_after(m_proxyConfiguration.readTimeout, asio::use_awaitable));
            if (lineSize == 0)
                break;

            size_t chunkSize = ParseChunkSize(std::string_view(storage.data(), lineSize));

            co_await asio::async_write(
                destination,
                buffer.data(0, lineSize),
                asio::cancel_after(m_proxyConfiguration.writeTimeout, asio::use_awaitable));
            buffer.consume(lineSize);

            totalBytes += co_await ForwardData(source, destination, storage, chunkSize);

            lineSize = co_await asio::async_read_until(
                source,
                buffer,
                '\n',
                asio::cancel_after(m_proxyConfiguration.readTimeout, asio::use_awaitable));
            if (lineSize == 0)
                break;

            co_await asio::async_write(
                destination,
                buffer.data(0, lineSize),
                asio::cancel_after(m_proxyConfiguration.writeTimeout, asio::use_awaitable));
            buffer.consume(lineSize);

            if (chunkSize == 0)
            {
                break;
            }
        }

        co_return totalBytes;
    }

    asio::awaitable<size_t> ProxyRequestHandler::WriteData(asio::ip::tcp::socket& socket, std::string_view buffer) const
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: buffer.size=%llu", buffer.size());

        if (buffer.empty())
            co_return 0;

        try
        {
            co_return co_await asio::async_write(
                socket,
                asio::buffer(buffer),
                asio::cancel_after(m_proxyConfiguration.writeTimeout, asio::use_awaitable));
        }
        catch (const std::exception& e)
        {
            DoTraceMessage(WppWarning, "Failed to write data: %s", e.what());
            throw;
        }
    }

    asio::awaitable<size_t> ProxyRequestHandler::WriteResponse(asio::ip::tcp::socket& socket, HttpResponseMessage response) const
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: StatusCode=%u", static_cast<int>(response.GetStatusCode()));

        if (!response.GetHeaders().Contains("Content-Length"))
        {
            response.GetHeaders().Add("Content-Length", std::to_string(response.GetBody().size()));
        }

        co_return co_await WriteData(socket, response.ToString());
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

    bool ProxyRequestHandler::IsRequestToWebServer(const Uri& uri) const
    {
        return IsLocalhost(uri.host) && uri.port == m_proxyConfiguration.port;
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
            return std::format("{}, 1.1 {}", *via, m_proxyId);
        }
        else
        {
            return std::format("1.1 {}", m_proxyId);
        }
    }

    size_t ProxyRequestHandler::ParseChunkSize(std::string_view line)
    {
        size_t chunkSize = 0;
        try
        {
            auto size = Trim(line.substr(0, line.find(';')));
            chunkSize = ParseHex<size_t>(size);
        }
        catch (const std::exception&)
        {
            throw HttpError(HttpStatusCode::BadRequest, "Invalid chunk size.");
        }

        if (chunkSize > maxChunkSize)
        {
            throw HttpError(HttpStatusCode::BadRequest, "Chunk size exceeds maximum allowed value.");
        }

        return chunkSize;
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

    HttpHeaders ProxyRequestHandler::FilterHeaders(const HttpHeaders& headers, std::function<bool(std::string_view name)> predicate)
    {
        HttpHeaders result;

        for (const auto& header : headers)
        {
            if (predicate(header.name))
            {
                result.Add(header.name, header.value);
            }
        }

        return result;
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

    bool ProxyRequestHandler::HasBasicCredentials(const ProxyConfiguration& configuration)
    {
        return !configuration.gatewayUsername.empty();
    }
}
