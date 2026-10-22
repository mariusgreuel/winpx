//
// ProxyRequestHandler.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
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
            size_t m_requestId = 0;
            std::string clientBuffer;
            std::unique_ptr<HttpReplayBuffer> replayBuffer;
            HttpMessageFraming framing;
            RequestStrategy strategy = RequestStrategy::NoBody;
            bool requestBodySent = false;
            bool responseHeadSent = false;
            bool continueSent = false;
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

        asio::awaitable<void> HandleRequest(RequestContext& context, asio::ip::tcp::socket& client, const HttpRequestMessage& request);
        asio::awaitable<void> HandleWebServerRequest(RequestContext& context, asio::ip::tcp::socket& client, const HttpRequestMessage& request);
        asio::awaitable<void> HandleProxyRequest(RequestContext& context, asio::ip::tcp::socket& client, const HttpRequestMessage& request);
        asio::awaitable<void> HandleConnectRequest(RequestContext& context, asio::ip::tcp::socket& client, const HttpRequestMessage& request);
        asio::awaitable<void> HandleStandardRequest(RequestContext& context, asio::ip::tcp::socket& client, const HttpRequestMessage& request);

        asio::awaitable<ConnectToUpstreamProxyResult> ConnectToUpstreamProxy(RequestContext& context, asio::ip::tcp::socket& client, const ProxyResolverResult& proxy, HttpRequestMessage request);
        asio::awaitable<HttpResponseMessage> SendRequest(RequestContext& context, asio::ip::tcp::socket& client, asio::ip::tcp::socket& server, const HttpRequestMessage& request);
        asio::awaitable<WaitForContinueResult> WaitForContinue(asio::ip::tcp::socket& server, std::string& buffer) const;
        asio::awaitable<HttpResponseMessage> ReceiveFinalResponse(asio::ip::tcp::socket& server, std::string& buffer) const;

        bool IsRequestToWebServer(const HttpRequestMessage& request) const;
        bool IsRequestToWebServer(const Uri& target) const;
        bool IsAuthenticated(const HttpRequestMessage& request, std::string_view headerName) const;
        bool IsSentViaThisProxy(const HttpRequestMessage& request) const;
        std::string CreateViaThisProxy(const HttpRequestMessage& request) const;

        RequestStrategy SelectRequestStrategy(const HttpRequestMessage& request, const HttpMessageFraming& framing, bool direct) const;
        static AuthenticationSchemes ParseAuthenticationOffer(const HttpResponseMessage& response);
        static std::vector<std::byte> ParseAuthenticationToken(const HttpResponseMessage& response, std::string_view scheme);
        static std::tuple<std::string_view, std::string_view> SplitAuthenticationHeader(std::string_view value);
        static HttpHeaders FilterHeaders(const HttpHeaders& headers, std::function<bool(std::string_view name)> predicate);
        static bool IsHopByHopHeader(std::string_view name);
        static bool IsConnectionClose(const HttpMessage& message);
        static bool HasConnectionOption(const std::vector<std::string_view>& connectionOptions, std::string_view name);
        static bool HasExpectContinue(const HttpMessage& message);
        static bool HasBasicCredentials(const ProxyConfiguration& configuration);

    private:
        const ProxyConfiguration& m_proxyConfiguration;
        ProxyResolver& m_proxyResolver;
        ProxyStatistics& m_proxyStatistics;
        std::string m_proxyId;
    };
}
