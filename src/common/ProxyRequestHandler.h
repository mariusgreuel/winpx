//
// ProxyRequestHandler.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include "HttpConnectionPool.h"
#include "HttpMessageFraming.h"
#include "HttpReplayBuffer.h"
#include "HttpRequestMessage.h"
#include "HttpResponseMessage.h"
#include "HttpTransport.h"
#include "ProxyResolver.h"
#include "ProxyStatistics.h"

namespace winpx
{
    class ProxyRequestHandler : private HttpTransport
    {
    public:
        explicit ProxyRequestHandler(const ProxyConfiguration& proxyConfiguration, ProxyResolver& proxyResolver, ProxyStatistics& proxyStatistics);
        ~ProxyRequestHandler();

        ProxyRequestHandler(const ProxyRequestHandler&) = delete;
        ProxyRequestHandler& operator=(const ProxyRequestHandler&) = delete;

        asio::awaitable<void> Handle(asio::ip::tcp::socket socket);
        HttpConnectionPool& GetConnectionPool() noexcept { return m_connectionPool; }

        enum class RequestStrategy
        {
            NoBody,
            StreamBody,
            StreamAndCaptureBody,
            ReplayBody,
            ClientExpect,
            InsertExpect,
        };

        struct RequestContext
        {
            RequestContext(asio::ip::tcp::socket& client) :
                client(client)
            {
            }

            void Reuse(size_t requestId)
            {
                m_requestId = requestId;
                request = {};
                requestFraming = {};
                requestStrategy = RequestStrategy::NoBody;
                replayBuffer.reset();
                requestBodySent = false;
                responseSent = false;
                continueSent = false;
                keepClientAlive = false;
            }

            asio::ip::tcp::socket& client;
            std::string clientBuffer;
            std::string serverBuffer;
            size_t m_requestId = 0;
            HttpRequestMessage request;
            HttpMessageFraming requestFraming;
            RequestStrategy requestStrategy = RequestStrategy::NoBody;
            std::unique_ptr<HttpReplayBuffer> replayBuffer;
            bool firstRequest = true;
            bool requestBodySent = false;
            bool responseSent = false;
            bool continueSent = false;
            bool keepClientAlive = false;
        };

    private:
        struct ConnectToUpstreamProxyResult
        {
            asio::ip::tcp::socket server;
            HttpResponseMessage response;
        };

        struct WaitForContinueResult
        {
            std::optional<HttpResponseMessage> response;
            bool timedOut = false;
        };

        asio::awaitable<void> HandleRequest(RequestContext& context);
        asio::awaitable<void> HandleWebServerRequest(RequestContext& context);
        asio::awaitable<void> HandleProxyRequest(RequestContext& context);
        asio::awaitable<void> HandleConnectRequest(RequestContext& context);
        asio::awaitable<void> HandleStandardRequest(RequestContext& context);

        asio::awaitable<ConnectToUpstreamProxyResult> ConnectToUpstreamProxy(RequestContext& context, const std::string& host, uint16_t port, HttpRequestMessage request);
        asio::awaitable<HttpResponseMessage> SendRequest(RequestContext& context, asio::ip::tcp::socket& server, const HttpRequestMessage& request);
        asio::awaitable<WaitForContinueResult> WaitForContinue(asio::ip::tcp::socket& server, std::string& buffer) const;
        asio::awaitable<HttpResponseMessage> ReceiveFinalResponse(asio::ip::tcp::socket& server, std::string& buffer) const;
        asio::awaitable<void> ForwardResponse(RequestContext& context, asio::ip::tcp::socket& server, const HttpResponseMessage& response, const std::string& connectionKey);

        bool IsRequestToWebServer(const HttpRequestMessage& request) const;
        bool IsRequestToWebServer(const Uri& target) const;
        bool IsAuthenticated(const HttpRequestMessage& request, std::string_view headerName) const;
        bool IsSentViaThisProxy(const HttpRequestMessage& request) const;
        std::string CreateViaThisProxy(const HttpRequestMessage& request) const;

        RequestStrategy SelectRequestStrategy(const HttpRequestMessage& request, const HttpMessageFraming& framing, bool direct) const;
        static HttpRequestMessage ParseRequestHead(std::string& buffer, size_t headSize);
        static HttpResponseMessage ParseResponseHead(std::string& buffer, size_t headSize);
        static HttpMessageFraming GetFraming(const HttpRequestMessage& request);
        static HttpMessageFraming GetFraming(const HttpResponseMessage& response);
        static AuthenticationSchemes ParseAuthenticationOffer(const HttpResponseMessage& response);
        static std::vector<std::byte> ParseAuthenticationToken(const HttpResponseMessage& response, std::string_view scheme);
        static std::tuple<std::string_view, std::string_view> SplitAuthenticationHeader(std::string_view value);
        static HttpHeaders GetSanitizedHeaders(const HttpMessage& message, const HttpMessageFraming& framing);
        static bool IsHopByHopHeader(std::string_view name);
        static bool IsConnectionClose(const HttpMessage& message);
        static bool HasConnectionOption(const std::vector<std::string_view>& connectionOptions, std::string_view name);
        static bool HasExpectContinue(const HttpMessage& message);
        static bool HasBasicCredentials(const ProxyConfiguration& configuration);
        static bool IsConnectionLost(const asio::error_code& ec);

    private:
        const ProxyConfiguration& m_proxyConfiguration;
        ProxyResolver& m_proxyResolver;
        ProxyStatistics& m_proxyStatistics;
        HttpConnectionPool m_connectionPool;
        std::string m_proxyId;
    };
}
