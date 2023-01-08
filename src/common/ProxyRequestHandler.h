//
// ProxyRequestHandler.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include "HttpRequestMessage.h"
#include "HttpResponseMessage.h"
#include "HttpTools.h"
#include "ProxyResolver.h"
#include "ProxyStatistics.h"

namespace winpx
{
    class ProxyRequestHandler : private HttpTools
    {
    public:
        explicit ProxyRequestHandler(const ProxyConfiguration& proxyConfiguration, ProxyResolver& proxyResolver, ProxyStatistics& proxyStatistics);
        ~ProxyRequestHandler();

        ProxyRequestHandler(const ProxyRequestHandler&) = delete;
        ProxyRequestHandler& operator=(const ProxyRequestHandler&) = delete;

        asio::awaitable<void> Handle(asio::ip::tcp::socket socket);

    private:
        struct ProxyConnection
        {
            asio::ip::tcp::socket socket;
            HttpResponseMessage response;
        };

        asio::awaitable<void> HandleWebServerRequest(asio::ip::tcp::socket& client, const HttpRequestMessage& request);
        asio::awaitable<void> HandleProxyRequest(asio::ip::tcp::socket& client, const HttpRequestMessage& request);
        asio::awaitable<void> HandleConnectRequest(asio::ip::tcp::socket& client, const HttpRequestMessage& request);
        asio::awaitable<void> HandleStandardRequest(asio::ip::tcp::socket& client, const HttpRequestMessage& request);

        asio::awaitable<ProxyConnection> ConnectToUpstreamProxy(const ProxyResolverResult& proxy, HttpRequestMessage request, bool hasBody);

        asio::awaitable<asio::ip::tcp::socket> ConnectTo(std::string_view host, uint16_t port);
        asio::awaitable<std::string> ReadBody(asio::ip::tcp::socket& client, const HttpMessage& message);
        asio::awaitable<void> ForwardBody(asio::ip::tcp::socket& upstream, asio::ip::tcp::socket& client, const HttpMessage& message);
        asio::awaitable<void> Tunnel(asio::ip::tcp::socket& client, asio::ip::tcp::socket& upstream, std::string_view clientLeftover, std::string_view upstreamLeftover);

        asio::awaitable<std::tuple<size_t, size_t>> RelayData(asio::ip::tcp::socket& client, asio::ip::tcp::socket& upstream) const;
        asio::awaitable<size_t> RelayDataOneWay(asio::ip::tcp::socket& source, asio::ip::tcp::socket& destination) const;

        asio::awaitable<std::string> ReadData(asio::ip::tcp::socket& socket, std::string& storage, size_t contentLength) const;
        asio::awaitable<std::string> ReadChunkedData(asio::ip::tcp::socket& socket, std::string& storage) const;

        asio::awaitable<size_t> ForwardData(asio::ip::tcp::socket& source, asio::ip::tcp::socket& destination, std::string& storage, size_t contentLength) const;
        asio::awaitable<size_t> ForwardChunkedData(asio::ip::tcp::socket& source, asio::ip::tcp::socket& destination, std::string& storage) const;

        asio::awaitable<size_t> WriteData(asio::ip::tcp::socket& socket, std::string_view buffer) const;
        asio::awaitable<size_t> WriteResponse(asio::ip::tcp::socket& socket, HttpResponseMessage response) const;

        bool IsRequestToWebServer(const HttpRequestMessage& request) const;
        bool IsRequestToWebServer(const Uri& uri) const;

        bool IsAuthenticated(const HttpRequestMessage& request, std::string_view headerName) const;
        bool IsSentViaThisProxy(const HttpRequestMessage& request) const;
        std::string CreateViaThisProxy(const HttpRequestMessage& request) const;

        static size_t ParseChunkSize(std::string_view line);
        static AuthenticationSchemes ParseAuthenticationOffer(const HttpResponseMessage& response);
        static std::vector<std::byte> ParseAuthenticationToken(const HttpResponseMessage& response, std::string_view scheme);
        static std::tuple<std::string_view, std::string_view> SplitAuthenticationHeader(std::string_view value);
        static HttpHeaders FilterHeaders(const HttpHeaders& headers, std::function<bool(std::string_view name)> predicate);
        static bool IsHopByHopHeader(std::string_view name);
        static bool HasBasicCredentials(const ProxyConfiguration& configuration);

    private:
        const ProxyConfiguration& m_proxyConfiguration;
        ProxyResolver& m_proxyResolver;
        ProxyStatistics& m_proxyStatistics;
        std::string m_proxyId;
    };
}
