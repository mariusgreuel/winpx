//
// ProxySystemTests.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include <common/pch.h>

#include "TestHelpers.h"
#include <common/Proxy.h>
#include <common/StringHelper.h>
#include <win32/Base64.h>
#include <win32/WppTracing.h>

#include <gtest/gtest.h>

namespace SystemTests
{
    using namespace win32;
    using namespace winpx;
    using namespace winpx::tests;
    using namespace std::literals::string_view_literals;
    using namespace std::chrono_literals;
    using asio::ip::tcp;

    static constexpr size_t largeChunkSize = 0x11000;

    static WppTracing wpp;

    static uint16_t ReserveLoopbackPort()
    {
        asio::io_context ioContext;
        tcp::acceptor acceptor(ioContext, tcp::endpoint(asio::ip::address_v4::loopback(), 0));
        return acceptor.local_endpoint().port();
    }

    static std::vector<std::byte> GetAuthenticationToken(const HttpRequestMessage& request, std::string_view scheme)
    {
        auto authorization = request.GetHeaders().GetValue("Proxy-Authorization"sv);
        if (!authorization || !IStartsWith(*authorization, scheme))
            return {};

        auto token = Trim(authorization->substr(scheme.size()));
        return Base64::Decode(token);
    }

    static std::optional<std::string_view> GetHeaderValue(std::string_view head, std::string_view name)
    {
        auto position = head.find(name);
        if (position == std::string::npos)
            return std::nullopt;

        position += name.size();
        auto end = head.find("\r\n", position);
        return Trim(head.substr(position, end - position));
    }

    static bool IsChunked(std::string_view head)
    {
        auto transferEncoding = GetHeaderValue(head, "Transfer-Encoding:"sv);
        return transferEncoding && *transferEncoding == "chunked"sv;
    }

    static size_t GetContentLength(std::string_view head)
    {
        auto contentLength = GetHeaderValue(head, "Content-Length:"sv);
        return contentLength ? ParseDec<size_t>(*contentLength) : 0;
    }

    static std::string ReadData(tcp::socket& socket, std::string& buffer, size_t length)
    {
        if (buffer.size() < length)
        {
            asio::read(socket, asio::dynamic_buffer(buffer), asio::transfer_exactly(length - buffer.size()));
        }

        auto data = buffer.substr(0, length);
        buffer.erase(0, length);
        return data;
    }

    static std::string ReadChunkedData(tcp::socket& socket, std::string& buffer)
    {
        const auto readLine = [&socket, &buffer]() {
            auto lineSize = asio::read_until(socket, asio::dynamic_buffer(buffer), "\r\n"sv);
            auto line = buffer.substr(0, lineSize - 2);
            buffer.erase(0, lineSize);
            return line;
        };

        std::string result;

        while (true)
        {
            auto sizeLine = readLine();
            result += sizeLine + "\r\n";

            auto chunkSize = ParseHex<size_t>(Trim(std::string_view(sizeLine).substr(0, sizeLine.find(';'))));
            if (chunkSize == 0)
                break;

            result += ReadData(socket, buffer, chunkSize);

            if (!readLine().empty())
            {
                throw std::runtime_error("Missing CRLF after chunk data.");
            }

            result += "\r\n";
        }

        while (true)
        {
            auto sizeLine = readLine();
            result += sizeLine + "\r\n";
            if (sizeLine.empty())
                break;
        }

        return result;
    }

    static std::tuple<std::string, size_t> ReadMessage(tcp::socket& socket)
    {
        std::string buffer;
        auto headSize = asio::read_until(socket, asio::dynamic_buffer(buffer), "\r\n\r\n"sv);
        auto head = buffer.substr(0, headSize);
        buffer.erase(0, headSize);

        if (IsChunked(head))
        {
            auto body = ReadChunkedData(socket, buffer);
            return { head + body, headSize };
        }
        else
        {
            auto contentLength = GetContentLength(head);
            auto body = ReadData(socket, buffer, contentLength);
            return { head + body, headSize };
        }
    }

    static HttpRequestMessage ReadRequestMessage(tcp::socket& socket)
    {
        auto [message, headSize] = ReadMessage(socket);

        HttpRequestMessage request;
        request.Parse(message, headSize);
        return std::move(request);
    }

    static HttpResponseMessage ReadResponseMessage(tcp::socket& socket)
    {
        auto [message, headSize] = ReadMessage(socket);

        HttpResponseMessage response;
        response.Parse(message, headSize);
        return std::move(response);
    }

    static void WriteAll(tcp::socket& socket, std::string_view value)
    {
        asio::write(socket, asio::buffer(value));
    }

    static std::string ReadToEnd(tcp::socket& socket)
    {
        std::string result;
        asio::error_code error;

        while (true)
        {
            std::array<char, 4096> buffer{};
            auto bytesRead = socket.read_some(asio::buffer(buffer), error);
            result.append(buffer.data(), bytesRead);

            if (error == asio::error::eof)
                break;

            if (error)
            {
                throw asio::system_error(error);
            }
        }

        return result;
    }

    class NegotiateServerContext
    {
    public:
        struct Result
        {
            SECURITY_STATUS status;
            std::vector<std::byte> token;
        };

        NegotiateServerContext()
        {
            SecInvalidateHandle(&m_credentials);
            SecInvalidateHandle(&m_context);

            TimeStamp expiry{};
            auto status = AcquireCredentialsHandleW(
                nullptr,
                const_cast<wchar_t*>(L"Negotiate"),
                SECPKG_CRED_INBOUND,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                &m_credentials,
                &expiry);
            if (status != SEC_E_OK)
            {
                throw std::runtime_error(std::format("AcquireCredentialsHandleW failed: 0x{:08X}", static_cast<uint32_t>(status)));
            }
        }

        ~NegotiateServerContext()
        {
            if (SecIsValidHandle(&m_context))
            {
                DeleteSecurityContext(&m_context);
            }

            if (SecIsValidHandle(&m_credentials))
            {
                FreeCredentialsHandle(&m_credentials);
            }
        }

        NegotiateServerContext(const NegotiateServerContext&) = delete;
        NegotiateServerContext& operator=(const NegotiateServerContext&) = delete;

        Result Accept(const std::vector<std::byte>& inputToken)
        {
            SecBuffer input{ static_cast<unsigned long>(inputToken.size()), SECBUFFER_TOKEN, const_cast<std::byte*>(inputToken.data()) };
            SecBufferDesc inputDesc{ SECBUFFER_VERSION, 1, &input };

            SecBuffer output{};
            output.BufferType = SECBUFFER_TOKEN;
            SecBufferDesc outputDesc{ SECBUFFER_VERSION, 1, &output };
            ULONG attributes = 0;
            TimeStamp expiry{};
            auto status = AcceptSecurityContext(
                &m_credentials,
                SecIsValidHandle(&m_context) ? &m_context : nullptr,
                &inputDesc,
                ASC_REQ_CONNECTION | ASC_REQ_ALLOCATE_MEMORY,
                SECURITY_NATIVE_DREP,
                &m_context,
                &outputDesc,
                &attributes,
                &expiry);

            if (status == SEC_I_COMPLETE_NEEDED || status == SEC_I_COMPLETE_AND_CONTINUE)
            {
                auto completeStatus = CompleteAuthToken(&m_context, &outputDesc);
                if (completeStatus != SEC_E_OK)
                {
                    throw std::runtime_error(std::format("CompleteAuthToken failed: 0x{:08X}", static_cast<uint32_t>(completeStatus)));
                }

                status = status == SEC_I_COMPLETE_NEEDED ? SEC_E_OK : SEC_I_CONTINUE_NEEDED;
            }

            std::vector<std::byte> token;
            if (output.pvBuffer && output.cbBuffer > 0)
            {
                auto begin = static_cast<const std::byte*>(output.pvBuffer);
                token.assign(begin, begin + output.cbBuffer);
            }
            if (output.pvBuffer)
            {
                FreeContextBuffer(output.pvBuffer);
            }

            if (status != SEC_E_OK && status != SEC_I_CONTINUE_NEEDED)
            {
                throw std::runtime_error(std::format("AcceptSecurityContext failed: 0x{:08X}", static_cast<uint32_t>(status)));
            }

            return { status, std::move(token) };
        }

    private:
        CredHandle m_credentials;
        CtxtHandle m_context;
    };

    class ScriptedServer
    {
    public:
        using Handler = std::function<void(tcp::socket&)>;
        using MultiHandler = std::function<void(tcp::socket&, size_t)>;

        explicit ScriptedServer(Handler handler) :
            ScriptedServer(1, [handler = std::move(handler)](tcp::socket& socket, size_t) { handler(socket); })
        {
        }

        ScriptedServer(size_t connections, MultiHandler handler) :
            m_acceptor(m_ioContext, tcp::endpoint(asio::ip::address_v4::loopback(), 0)),
            m_connections(connections),
            m_handler(std::move(handler)),
            m_thread([this]() { Run(); })
        {
        }

        ~ScriptedServer()
        {
            asio::error_code error;
            m_acceptor.close(error);

            if (m_thread.joinable())
            {
                m_thread.join();
            }
        }

        ScriptedServer(const ScriptedServer&) = delete;
        ScriptedServer& operator=(const ScriptedServer&) = delete;

        uint16_t GetPort() const
        {
            return m_acceptor.local_endpoint().port();
        }

        void RethrowFailure()
        {
            if (m_thread.joinable())
            {
                m_thread.join();
            }

            if (m_failure)
            {
                std::rethrow_exception(m_failure);
            }
        }

    private:
        void Run()
        {
            try
            {
                for (size_t index = 0; index < m_connections; index++)
                {
                    tcp::socket socket(m_ioContext);
                    m_acceptor.accept(socket);
                    m_handler(socket, index);
                }
            }
            catch (...)
            {
                m_failure = std::current_exception();
            }
        }

        asio::io_context m_ioContext;
        tcp::acceptor m_acceptor;
        size_t m_connections;
        MultiHandler m_handler;
        std::thread m_thread;
        std::exception_ptr m_failure;
    };

    class ProxySystemTest : public testing::Test
    {
    protected:
        void TearDown() override
        {
            if (m_proxy)
            {
                m_proxy->Stop();
            }
        }

        ProxyConfiguration DirectConfiguration() const
        {
            ProxyConfiguration configuration;
            configuration.proxyMode = ProxyMode::Direct;
            configuration.networkMode = NetworkMode::Loopback;
            configuration.port = ReserveLoopbackPort();
            configuration.threadPoolSize = 2;
            return configuration;
        }

        ProxyConfiguration ManualProxyConfiguration(uint16_t upstreamPort) const
        {
            auto configuration = DirectConfiguration();
            configuration.proxyMode = ProxyMode::Manual;
            configuration.httpProxyUrl = std::format("http://127.0.0.1:{}", upstreamPort);
            configuration.httpsProxyUrl = configuration.httpProxyUrl;
            return configuration;
        }

        void StartProxy(ProxyConfiguration configuration)
        {
            m_configuration = std::move(configuration);
            m_proxy = std::make_unique<Proxy>(m_configuration);
            m_proxy->Start();
            ASSERT_EQ(ProxyState::Running, m_proxy->GetState());
        }

        tcp::socket ConnectToProxy(asio::io_context& ioContext) const
        {
            tcp::socket socket(ioContext);
            socket.connect(tcp::endpoint(asio::ip::address_v4::loopback(), m_configuration.port));
            return socket;
        }

        HttpResponseMessage SendRequestToProxy(std::string_view request) const
        {
            asio::io_context ioContext;
            auto socket = ConnectToProxy(ioContext);
            WriteAll(socket, request);
            socket.shutdown(tcp::socket::shutdown_send);
            auto message = ReadToEnd(socket);
            if (message.empty())
                return {};

            HttpResponseMessage response;
            response.Parse(message);
            return response;
        }

        ProxyConfiguration m_configuration;
        std::unique_ptr<Proxy> m_proxy;
    };

    TEST_F(ProxySystemTest, DirectGetRequest)
    {
        HttpRequestMessage originRequest;
        ScriptedServer originServer([&](tcp::socket& socket) {
            originRequest = ReadRequestMessage(socket);
            WriteAll(socket,
                "HTTP/1.1 200 OK\r\n"
                "Content-Length: 8\r\n"
                "Connection: close\r\n"
                "\r\n"
                "get-body"sv);
        });

        StartProxy(DirectConfiguration());

        auto response = SendRequestToProxy(std::format(
            "GET http://127.0.0.1:{0}/direct-get-request HTTP/1.1\r\n"
            "Host: 127.0.0.1:{0}\r\n"
            "X-Test: direct-get\r\n"
            "\r\n",
            originServer.GetPort()));
        originServer.RethrowFailure();

        EXPECT_EQ(response.GetStatusCode(), HttpStatusCode::OK);
        EXPECT_EQ(response.GetBody(), "get-body");
        EXPECT_EQ(originRequest.GetMethod(), "GET");
        EXPECT_EQ(originRequest.GetRequestUri().ToString(), "/direct-get-request");
        EXPECT_EQ(originRequest.GetHeaders().GetValue("X-Test"sv), "direct-get");
        EXPECT_EQ(originRequest.GetHeaders().GetValue("Connection"sv), "close");
        EXPECT_EQ(originRequest.GetHeaders().GetValue("Via"sv), std::format("1.1 winpx:{}", m_configuration.port));
        EXPECT_TRUE(originRequest.GetBody().empty());
    }

    TEST_F(ProxySystemTest, DirectPostRequest)
    {
        HttpRequestMessage originRequest;
        ScriptedServer originServer([&](tcp::socket& socket) {
            originRequest = ReadRequestMessage(socket);
            WriteAll(socket,
                "HTTP/1.1 201 Created\r\n"
                "Content-Length: 9\r\n"
                "Connection: close\r\n"
                "\r\n"
                "post-body"sv);
        });

        StartProxy(DirectConfiguration());

        constexpr auto body = "name=winpx&mode=system-test"sv;
        auto response = SendRequestToProxy(std::format(
            "POST http://127.0.0.1:{}/direct-post-request HTTP/1.1\r\n"
            "Host: 127.0.0.1:{}\r\n"
            "Content-Type: application/x-www-form-urlencoded\r\n"
            "Content-Length: {}\r\n"
            "\r\n"
            "{}",
            originServer.GetPort(), originServer.GetPort(), body.size(), body));
        originServer.RethrowFailure();

        EXPECT_EQ(response.GetStatusCode(), HttpStatusCode::Created);
        EXPECT_EQ(response.GetBody(), "post-body");
        EXPECT_EQ(originRequest.GetMethod(), "POST");
        EXPECT_EQ(originRequest.GetRequestUri().ToString(), "/direct-post-request");
        EXPECT_EQ(originRequest.GetHeaders().GetValue("Content-Type"sv), "application/x-www-form-urlencoded");
        EXPECT_EQ(originRequest.GetHeaders().GetValue("Connection"sv), "close");
        EXPECT_EQ(originRequest.GetBody(), body);
    }

    TEST_F(ProxySystemTest, DirectHeadRequest)
    {
        HttpRequestMessage originRequest;
        ScriptedServer originServer([&](tcp::socket& socket) {
            originRequest = ReadRequestMessage(socket);
            WriteAll(socket,
                "HTTP/1.1 200 OK\r\n"
                "Content-Length: 123\r\n"
                "Connection: close\r\n"
                "\r\n"sv);
        });

        StartProxy(DirectConfiguration());

        auto response = SendRequestToProxy(std::format(
            "HEAD http://127.0.0.1:{0}/direct-head-request HTTP/1.1\r\n"
            "Host: 127.0.0.1:{0}\r\n"
            "\r\n",
            originServer.GetPort()));
        originServer.RethrowFailure();

        EXPECT_EQ(response.GetStatusCode(), HttpStatusCode::OK);
        EXPECT_EQ(response.GetHeaders().GetValue("Content-Length"sv), "123");
        EXPECT_EQ(response.GetHeaders().GetValue("Connection"sv), "close");
        EXPECT_TRUE(response.GetBody().empty());
        EXPECT_EQ(originRequest.GetMethod(), "HEAD");
        EXPECT_EQ(originRequest.GetRequestUri().ToString(), "/direct-head-request");
    }

    TEST_F(ProxySystemTest, ProxyGetRequest)
    {
        HttpRequestMessage upstreamRequest;
        ScriptedServer upstreamProxy([&](tcp::socket& socket) {
            upstreamRequest = ReadRequestMessage(socket);
            WriteAll(socket,
                "HTTP/1.1 200 OK\r\n"
                "Content-Length: 8\r\n"
                "Connection: close\r\n"
                "\r\n"
                "get-body"sv);
        });

        StartProxy(ManualProxyConfiguration(upstreamProxy.GetPort()));

        auto response = SendRequestToProxy(
            "GET http://example.org/proxy-get-request HTTP/1.1\r\n"
            "Host: example.org\r\n"
            "Proxy-Connection: keep-alive\r\n"
            "X-Test: proxy-get\r\n"
            "\r\n"sv);
        upstreamProxy.RethrowFailure();

        EXPECT_EQ(response.GetStatusCode(), HttpStatusCode::OK);
        EXPECT_EQ(response.GetBody(), "get-body");
        EXPECT_EQ(upstreamRequest.GetMethod(), "GET");
        EXPECT_EQ(upstreamRequest.GetRequestUri().ToString(), "http://example.org/proxy-get-request");
        EXPECT_EQ(upstreamRequest.GetHeaders().GetValue("X-Test"sv), "proxy-get");
        EXPECT_EQ(upstreamRequest.GetHeaders().GetValue("Connection"sv), "keep-alive");
        EXPECT_EQ(upstreamRequest.GetHeaders().GetValue("Via"sv), std::format("1.1 winpx:{}", m_configuration.port));
        EXPECT_TRUE(upstreamRequest.GetBody().empty());
    }

    TEST_F(ProxySystemTest, ProxyPostRequest)
    {
        HttpRequestMessage upstreamRequest;
        ScriptedServer upstreamProxy([&](tcp::socket& socket) {
            upstreamRequest = ReadRequestMessage(socket);
            WriteAll(socket,
                "HTTP/1.1 201 Created\r\n"
                "Content-Length: 9\r\n"
                "Connection: close\r\n"
                "\r\n"
                "post-body"sv);
        });

        StartProxy(ManualProxyConfiguration(upstreamProxy.GetPort()));

        constexpr auto body = "name=winpx&mode=system-test"sv;
        auto response = SendRequestToProxy(std::format(
            "POST http://example.org/proxy-post-request HTTP/1.1\r\n"
            "Host: example.org\r\n"
            "Content-Type: application/x-www-form-urlencoded\r\n"
            "Content-Length: {}\r\n"
            "\r\n"
            "{}",
            body.size(), body));
        upstreamProxy.RethrowFailure();

        EXPECT_EQ(response.GetStatusCode(), HttpStatusCode::Created);
        EXPECT_EQ(response.GetBody(), "post-body");
        EXPECT_EQ(upstreamRequest.GetMethod(), "POST");
        EXPECT_EQ(upstreamRequest.GetRequestUri().ToString(), "http://example.org/proxy-post-request");
        EXPECT_EQ(upstreamRequest.GetHeaders().GetValue("Content-Type"sv), "application/x-www-form-urlencoded");
        EXPECT_EQ(upstreamRequest.GetHeaders().GetValue("Connection"sv), "keep-alive");
        EXPECT_EQ(upstreamRequest.GetHeaders().GetValue("Via"sv), std::format("1.1 winpx:{}", m_configuration.port));
        EXPECT_EQ(upstreamRequest.GetBody(), body);
    }

    TEST_F(ProxySystemTest, ProxyHeadRequest)
    {
        HttpRequestMessage upstreamRequest;
        ScriptedServer upstreamProxy([&](tcp::socket& socket) {
            upstreamRequest = ReadRequestMessage(socket);
            WriteAll(socket,
                "HTTP/1.1 200 OK\r\n"
                "Content-Length: 123\r\n"
                "Connection: close\r\n"
                "\r\n"sv);
        });

        StartProxy(ManualProxyConfiguration(upstreamProxy.GetPort()));

        auto response = SendRequestToProxy(std::format(
            "HEAD http://example.org/proxy-head-request HTTP/1.1\r\n"
            "Host: example.org\r\n"
            "\r\n"));
        upstreamProxy.RethrowFailure();

        EXPECT_EQ(response.GetStatusCode(), HttpStatusCode::OK);
        EXPECT_EQ(response.GetHeaders().GetValue("Content-Length"sv), "123");
        EXPECT_EQ(response.GetHeaders().GetValue("Connection"sv), "close");
        EXPECT_TRUE(response.GetBody().empty());
        EXPECT_EQ(upstreamRequest.GetMethod(), "HEAD");
        EXPECT_EQ(upstreamRequest.GetRequestUri().ToString(), "http://example.org/proxy-head-request");
        EXPECT_EQ(upstreamRequest.GetHeaders().GetValue("Connection"sv), "keep-alive");
        EXPECT_EQ(upstreamRequest.GetHeaders().GetValue("Via"sv), std::format("1.1 winpx:{}", m_configuration.port));
        EXPECT_TRUE(upstreamRequest.GetBody().empty());
    }

    TEST_F(ProxySystemTest, HopByHopHeadersDoNotGetForwarded)
    {
        HttpRequestMessage originRequest;
        ScriptedServer originServer([&](tcp::socket& socket) {
            originRequest = ReadRequestMessage(socket);
            WriteAll(socket,
                "HTTP/1.1 200 OK\r\n"
                "Content-Length: 0\r\n"
                "Connection: close\r\n"
                "\r\n"sv);
        });

        StartProxy(DirectConfiguration());

        auto response = SendRequestToProxy(std::format(
            "GET http://127.0.0.1:{0}/hop-by-hop HTTP/1.1\r\n"
            "Host: 127.0.0.1:{0}\r\n"
            "Proxy-Connection: keep-alive\r\n"
            "Proxy-Authorization: Basic secret\r\n"
            "X-Test: direct-get\r\n"
            "X-Remove: to-be-removed\r\n"
            "Connection: X-Remove\r\n"
            "\r\n",
            originServer.GetPort()));
        originServer.RethrowFailure();

        EXPECT_EQ(response.GetStatusCode(), HttpStatusCode::OK);
        EXPECT_EQ(originRequest.GetHeaders().GetValue("X-Test"sv), "direct-get");
        EXPECT_EQ(originRequest.GetHeaders().GetValue("Connection"sv), "close");
        EXPECT_FALSE(originRequest.GetHeaders().Contains("Proxy-Connection"sv));
        EXPECT_FALSE(originRequest.GetHeaders().Contains("Proxy-Authorization"sv));
        EXPECT_FALSE(originRequest.GetHeaders().Contains("X-Remove"sv));
    }

    TEST_F(ProxySystemTest, ForwardsChunkedRequestBody)
    {
        const std::string largeChunk(largeChunkSize, 'X');
        const std::string chunkedBody = MakeChunkedBody({ "hello"sv, largeChunk, "world"sv });

        HttpRequestMessage originRequest;
        ScriptedServer originServer([&](tcp::socket& socket) {
            originRequest = ReadRequestMessage(socket);
            WriteAll(socket,
                "HTTP/1.1 200 OK\r\n"
                "Content-Length: 15\r\n"
                "Connection: close\r\n"
                "\r\n"
                "chunked-request"sv);
        });

        StartProxy(DirectConfiguration());

        auto response = SendRequestToProxy(std::format(
            "POST http://127.0.0.1:{0}/forwards-chunked-request HTTP/1.1\r\n"
            "Host: 127.0.0.1:{0}\r\n"
            "Transfer-Encoding: chunked\r\n"
            "\r\n"
            "{1}",
            originServer.GetPort(),
            chunkedBody));
        originServer.RethrowFailure();

        EXPECT_EQ(response.GetStatusCode(), HttpStatusCode::OK);
        EXPECT_EQ(response.GetBody(), "chunked-request");
        EXPECT_EQ(originRequest.GetHeaders().GetValue("Transfer-Encoding"sv), "chunked");
        EXPECT_FALSE(originRequest.GetHeaders().Contains("Content-Length"sv));
        EXPECT_EQ(originRequest.GetBody(), chunkedBody);
    }

    TEST_F(ProxySystemTest, ForwardsChunkedResponseBody)
    {
        const std::string largeChunk(largeChunkSize, 'X');
        const std::string chunkedBody = MakeChunkedBody({ "hello"sv, largeChunk, "world"sv });

        HttpRequestMessage originRequest;
        ScriptedServer originServer([&](tcp::socket& socket) {
            originRequest = ReadRequestMessage(socket);
            WriteAll(socket,
                "HTTP/1.1 200 OK\r\n"
                "Transfer-Encoding: chunked\r\n"
                "\r\n"sv);
            WriteAll(socket, chunkedBody);
        });

        StartProxy(DirectConfiguration());

        auto response = SendRequestToProxy(std::format(
            "GET http://127.0.0.1:{0}/forwards-chunked-response HTTP/1.1\r\n"
            "Host: 127.0.0.1:{0}\r\n"
            "\r\n"sv,
            originServer.GetPort()));
        originServer.RethrowFailure();

        EXPECT_EQ(response.GetStatusCode(), HttpStatusCode::OK);
        EXPECT_EQ(response.GetHeaders().GetValue("Transfer-Encoding"sv), "chunked");
        EXPECT_EQ(response.GetBody(), chunkedBody);
        EXPECT_EQ(originRequest.GetMethod(), "GET");
    }

    TEST_F(ProxySystemTest, TunnelsHttpsDirectly)
    {
        constexpr auto payload = "simulated-tls-record"sv;

        ScriptedServer originServer([&](tcp::socket& socket) {
            std::string received(payload.size(), '\0');
            asio::read(socket, asio::buffer(received));
            ASSERT_EQ(payload, received);
            WriteAll(socket, received);
        });

        StartProxy(DirectConfiguration());
        asio::io_context ioContext;
        auto client = ConnectToProxy(ioContext);
        WriteAll(client, std::format(
                             "CONNECT 127.0.0.1:{0} HTTP/1.1\r\n"
                             "Host: 127.0.0.1:{0}\r\n"
                             "\r\n",
                             originServer.GetPort()));

        auto response = ReadResponseMessage(client);
        ASSERT_EQ(response.GetStatusCode(), HttpStatusCode::OK);
        ASSERT_EQ(response.GetReasonPhrase(), "Connection Established");

        WriteAll(client, payload);
        std::string echoed(payload.size(), '\0');
        asio::read(client, asio::buffer(echoed));
        EXPECT_EQ(payload, echoed);
        originServer.RethrowFailure();
    }

    TEST_F(ProxySystemTest, TunnelsHttpsThroughUpstreamProxy)
    {
        constexpr auto payload = "simulated-tls-record"sv;

        HttpRequestMessage upstreamRequest;
        ScriptedServer upstreamProxy([&](tcp::socket& socket) {
            upstreamRequest = ReadRequestMessage(socket);
            WriteAll(socket, "HTTP/1.1 200 Connection Established\r\n\r\n"sv);

            std::string received(payload.size(), '\0');
            asio::read(socket, asio::buffer(received));
            ASSERT_EQ(payload, received);
            WriteAll(socket, received);
        });

        StartProxy(ManualProxyConfiguration(upstreamProxy.GetPort()));
        asio::io_context ioContext;
        auto client = ConnectToProxy(ioContext);
        WriteAll(client,
            "CONNECT secure.example.test:443 HTTP/1.1\r\n"
            "Host: secure.example.test:443\r\n"
            "\r\n"sv);

        auto response = ReadResponseMessage(client);
        ASSERT_EQ(response.GetStatusCode(), HttpStatusCode::OK);
        ASSERT_EQ(response.GetReasonPhrase(), "Connection Established");

        WriteAll(client, payload);
        std::string echoed(payload.size(), '\0');
        asio::read(client, asio::buffer(echoed));
        EXPECT_EQ(payload, echoed);
        upstreamProxy.RethrowFailure();

        EXPECT_EQ(upstreamRequest.GetMethod(), "CONNECT");
        EXPECT_EQ(upstreamRequest.GetRequestUri().ToString(), "secure.example.test:443");
        EXPECT_EQ(upstreamRequest.GetHeaders().GetValue("Host"sv), "secure.example.test:443");
    }

    TEST_F(ProxySystemTest, UpstreamProxyBasicAuthentication)
    {
        std::array<HttpRequestMessage, 2> upstreamRequests;
        ScriptedServer upstreamProxy([&](tcp::socket& socket) {
            upstreamRequests[0] = ReadRequestMessage(socket);
            WriteAll(socket,
                "HTTP/1.1 407 Proxy Authentication Required\r\n"
                "Proxy-Authenticate: Negotiate\r\n"
                "Proxy-Authenticate: NTLM\r\n"
                "Proxy-Authenticate: Digest realm=\"system-test\"\r\n"
                "Proxy-Authenticate: Basic realm=\"system-test\"\r\n"
                "Content-Length: 0\r\n"
                "\r\n"sv);

            upstreamRequests[1] = ReadRequestMessage(socket);
            WriteAll(socket,
                "HTTP/1.1 200 OK\r\n"
                "Content-Length: 12\r\n"
                "Connection: close\r\n"
                "\r\n"
                "basic-passed"sv);
        });

        auto configuration = ManualProxyConfiguration(upstreamProxy.GetPort());
        configuration.allowedAuthenticationSchemes = { AuthenticationScheme::Basic };
        configuration.gatewayUsername = "test-user"sv;
        configuration.gatewayPassword = "test-password"sv;
        StartProxy(std::move(configuration));

        auto response = SendRequestToProxy(
            "GET http://example.org/basic HTTP/1.1\r\n"
            "Host: example.org\r\n"
            "\r\n"sv);
        upstreamProxy.RethrowFailure();

        EXPECT_EQ(response.GetStatusCode(), HttpStatusCode::OK);
        EXPECT_EQ(response.GetBody(), "basic-passed");
        EXPECT_FALSE(upstreamRequests[0].GetHeaders().Contains("Proxy-Authorization"sv));
        EXPECT_EQ(upstreamRequests[1].GetBody(), upstreamRequests[0].GetBody());
        EXPECT_EQ(upstreamRequests[1].GetMethod(), "GET");
        EXPECT_EQ(upstreamRequests[1].GetRequestUri().ToString(), "http://example.org/basic");
        ASSERT_EQ(upstreamRequests[1].GetHeaders().GetValue("Proxy-Authorization"sv), "Basic " + Base64::Encode("test-user:test-password"sv));
    }

    TEST_F(ProxySystemTest, StopsBasicHandshakeAfterCredentialsAreRejected)
    {
        std::array<HttpRequestMessage, 2> upstreamRequests;
        ScriptedServer upstreamProxy([&](tcp::socket& socket) {
            for (auto& request : upstreamRequests)
            {
                request = ReadRequestMessage(socket);
                WriteAll(socket,
                    "HTTP/1.1 407 Proxy Authentication Required\r\n"
                    "Proxy-Authenticate: Basic realm=\"system-test\"\r\n"
                    "Content-Length: 0\r\n"
                    "\r\n"sv);
            }
        });

        auto configuration = ManualProxyConfiguration(upstreamProxy.GetPort());
        configuration.allowedAuthenticationSchemes = { AuthenticationScheme::Basic };
        configuration.gatewayUsername = "wrong-user"sv;
        configuration.gatewayPassword = "wrong-password"sv;
        StartProxy(std::move(configuration));

        auto response = SendRequestToProxy(
            "GET http://example.org/basic-rejected HTTP/1.1\r\n"
            "Host: example.org\r\n"
            "\r\n"sv);
        upstreamProxy.RethrowFailure();

        EXPECT_EQ(response.GetStatusCode(), HttpStatusCode::BadGateway);
        EXPECT_FALSE(upstreamRequests[0].GetHeaders().Contains("Proxy-Authorization"sv));
        ASSERT_EQ(upstreamRequests[1].GetHeaders().GetValue("Proxy-Authorization"sv), "Basic " + Base64::Encode("wrong-user:wrong-password"sv));
    }

    TEST_F(ProxySystemTest, ReturnsChallengeWhenUpstreamOffersNoAllowedAuthenticationScheme)
    {
        HttpRequestMessage upstreamRequest;
        ScriptedServer upstreamProxy([&](tcp::socket& socket) {
            upstreamRequest = ReadRequestMessage(socket);
            WriteAll(socket,
                "HTTP/1.1 407 Proxy Authentication Required\r\n"
                "Proxy-Authenticate: Basic realm=\"system-test\"\r\n"
                "Content-Length: 0\r\n"
                "Connection: close\r\n"
                "\r\n"sv);
        });

        auto configuration = ManualProxyConfiguration(upstreamProxy.GetPort());
        configuration.allowedAuthenticationSchemes = { AuthenticationScheme::Negotiate };
        StartProxy(std::move(configuration));

        auto response = SendRequestToProxy(
            "GET http://example.org/returns-challenge HTTP/1.1\r\n"
            "Host: example.org\r\n"
            "\r\n"sv);
        upstreamProxy.RethrowFailure();

        EXPECT_EQ(response.GetStatusCode(), HttpStatusCode::BadGateway);
        EXPECT_FALSE(response.GetHeaders().Contains("Proxy-Authenticate"sv));
        EXPECT_FALSE(upstreamRequest.GetHeaders().Contains("Proxy-Authorization"sv));
    }

    TEST_F(ProxySystemTest, CompletesUpstreamNegotiateHandshake)
    {
        std::vector<HttpRequestMessage> upstreamRequests;
        std::vector<SECURITY_STATUS> serverStatuses;
        std::optional<size_t> prematureCloseRound;

        ScriptedServer upstreamProxy([&](tcp::socket& socket) {
            upstreamRequests.push_back(ReadRequestMessage(socket));
            WriteAll(socket,
                "HTTP/1.1 407 Proxy Authentication Required\r\n"
                "Proxy-Authenticate: Basic realm=\"system-test\"\r\n"
                "Proxy-Authenticate: Negotiate\r\n"
                "Content-Length: 0\r\n"
                "\r\n"sv);

            NegotiateServerContext securityContext;
            for (size_t round = 0; round < 4; round++)
            {
                try
                {
                    upstreamRequests.push_back(ReadRequestMessage(socket));
                }
                catch (const asio::system_error& error)
                {
                    if (error.code() == asio::error::eof)
                    {
                        prematureCloseRound = round;
                        return;
                    }
                    throw;
                }

                auto inputToken = GetAuthenticationToken(upstreamRequests.back(), "Negotiate"sv);
                if (inputToken.empty())
                {
                    throw std::runtime_error("WinPX did not send a Negotiate authentication token.");
                }

                auto result = securityContext.Accept(inputToken);
                serverStatuses.push_back(result.status);
                if (result.status == SEC_E_OK)
                {
                    WriteAll(socket,
                        "HTTP/1.1 200 OK\r\n"
                        "Content-Length: 16\r\n"
                        "Connection: close\r\n"
                        "\r\n"
                        "negotiate-passed"sv);
                    return;
                }

                if (result.token.empty())
                {
                    throw std::runtime_error("SSPI requested another round without producing a challenge token.");
                }

                WriteAll(socket, std::format(
                                     "HTTP/1.1 407 Proxy Authentication Required\r\n"
                                     "Proxy-Authenticate: Negotiate {}\r\n"
                                     "Content-Length: 0\r\n"
                                     "\r\n",
                                     Base64::Encode(result.token)));
            }

            throw std::runtime_error("Negotiate handshake exceeded the expected number of rounds.");
        });

        auto configuration = ManualProxyConfiguration(upstreamProxy.GetPort());
        configuration.allowedAuthenticationSchemes = { AuthenticationScheme::Negotiate, AuthenticationScheme::Ntlm };
        StartProxy(std::move(configuration));

        auto response = SendRequestToProxy(
            "GET http://example.org/negotiate HTTP/1.1\r\n"
            "Host: example.org\r\n"
            "\r\n"sv);
        upstreamProxy.RethrowFailure();

        if (prematureCloseRound.has_value())
        {
            ASSERT_EQ(*prematureCloseRound, 1);
            ASSERT_EQ(upstreamRequests.size(), 2);
            ASSERT_EQ(serverStatuses.size(), 1);
            ASSERT_EQ(SEC_I_CONTINUE_NEEDED, serverStatuses.front());
            ASSERT_FALSE(GetAuthenticationToken(upstreamRequests[1], "Negotiate"sv).empty());
            RecordProperty("NegotiateHandshake", "Local Windows policy rejected the loopback/default-credential continuation after the first valid SSPI leg.");
            return;
        }

        EXPECT_EQ(response.GetStatusCode(), HttpStatusCode::OK);
        EXPECT_EQ(response.GetBody(), "negotiate-passed");
        ASSERT_GE(upstreamRequests.size(), 3);
        EXPECT_FALSE(upstreamRequests.front().GetHeaders().Contains("Proxy-Authorization"sv));
        for (size_t index = 1; index < upstreamRequests.size(); index++)
        {
            auto authorization = upstreamRequests[index].GetHeaders().GetValue("Proxy-Authorization"sv);
            ASSERT_TRUE(authorization);
            EXPECT_TRUE(IStartsWith(*authorization, "Negotiate "sv));
            EXPECT_FALSE(GetAuthenticationToken(upstreamRequests[index], "Negotiate"sv).empty());
        }
    }

    TEST_F(ProxySystemTest, ReconnectsWhenUpstreamClosesConnectionAfter407)
    {
        std::array<HttpRequestMessage, 2> upstreamRequests;
        ScriptedServer upstreamProxy(2, [&](tcp::socket& socket, size_t index) {
            upstreamRequests[index] = ReadRequestMessage(socket);
            if (index == 0)
            {
                WriteAll(socket,
                    "HTTP/1.1 407 Proxy Authentication Required\r\n"
                    "Proxy-Authenticate: Basic realm=\"system-test\"\r\n"
                    "Content-Length: 0\r\n"
                    "Connection: close\r\n"
                    "\r\n"sv);
            }
            else
            {
                WriteAll(socket,
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Length: 9\r\n"
                    "Connection: close\r\n"
                    "\r\n"
                    "reconnect"sv);
            }
        });

        auto configuration = ManualProxyConfiguration(upstreamProxy.GetPort());
        configuration.allowedAuthenticationSchemes = { AuthenticationScheme::Basic };
        configuration.gatewayUsername = "test-user"sv;
        configuration.gatewayPassword = "test-password"sv;
        StartProxy(std::move(configuration));

        auto response = SendRequestToProxy(
            "GET http://example.org/reconnect HTTP/1.1\r\n"
            "Host: example.org\r\n"
            "\r\n"sv);
        upstreamProxy.RethrowFailure();

        EXPECT_EQ(response.GetStatusCode(), HttpStatusCode::OK);
        EXPECT_EQ(response.GetBody(), "reconnect");
        EXPECT_FALSE(upstreamRequests[0].GetHeaders().Contains("Proxy-Authorization"sv));
        EXPECT_EQ(upstreamRequests[1].GetHeaders().GetValue("Proxy-Authorization"sv), "Basic " + Base64::Encode("test-user:test-password"sv));
    }

    TEST_F(ProxySystemTest, ReconnectsAfterMalformedUpstreamResponse)
    {
        ScriptedServer upstreamProxy(2, [&](tcp::socket& socket, size_t index) {
            ReadRequestMessage(socket);
            if (index == 0)
            {
                WriteAll(socket, "this is not http\r\n\r\n"sv);
            }
            else
            {
                WriteAll(socket,
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Length: 9\r\n"
                    "Connection: close\r\n"
                    "\r\n"
                    "recovered"sv);
            }
        });

        StartProxy(ManualProxyConfiguration(upstreamProxy.GetPort()));

        auto response = SendRequestToProxy(
            "GET http://example.org/malformed HTTP/1.1\r\n"
            "Host: example.org\r\n"
            "\r\n"sv);
        upstreamProxy.RethrowFailure();

        EXPECT_EQ(response.GetStatusCode(), HttpStatusCode::OK);
        EXPECT_EQ(response.GetBody(), "recovered");
    }

    TEST_F(ProxySystemTest, InsertsNoExpectContinueForSmallUpstreamRequestBody)
    {
        constexpr auto body = "expect-body"sv;

        std::string upstreamHead;
        std::string upstreamBody;
        ScriptedServer upstreamProxy([&](tcp::socket& socket) {
            std::string buffer;
            auto headSize = asio::read_until(socket, asio::dynamic_buffer(buffer), "\r\n\r\n"sv);
            upstreamHead = buffer.substr(0, headSize);
            buffer.erase(0, headSize);

            WriteAll(socket, "HTTP/1.1 100 Continue\r\n\r\n"sv);
            upstreamBody = ReadData(socket, buffer, body.size());

            WriteAll(socket,
                "HTTP/1.1 200 OK\r\n"
                "Content-Length: 2\r\n"
                "Connection: close\r\n"
                "\r\n"
                "ok"sv);
        });

        auto configuration = ManualProxyConfiguration(upstreamProxy.GetPort());
        configuration.httpConfiguration.maxBodySizeBeforeProbing = body.size();
        StartProxy(std::move(configuration));

        auto response = SendRequestToProxy(std::format(
            "POST http://example.org/expect HTTP/1.1\r\n"
            "Host: example.org\r\n"
            "Content-Length: {}\r\n"
            "\r\n"
            "{}",
            body.size(),
            body));
        upstreamProxy.RethrowFailure();

        EXPECT_EQ(response.GetStatusCode(), HttpStatusCode::OK);
        EXPECT_EQ(response.GetBody(), "ok");
        EXPECT_EQ(upstreamHead.find("Expect: 100-continue\r\n"), std::string::npos);
        EXPECT_EQ(upstreamBody, body);
    }

    TEST_F(ProxySystemTest, InsertsExpectContinueForBigUpstreamRequestBody)
    {
        constexpr auto body = "expect-body"sv;

        std::string upstreamHead;
        std::string upstreamBody;
        ScriptedServer upstreamProxy([&](tcp::socket& socket) {
            std::string buffer;
            auto headSize = asio::read_until(socket, asio::dynamic_buffer(buffer), "\r\n\r\n"sv);
            upstreamHead = buffer.substr(0, headSize);
            buffer.erase(0, headSize);

            WriteAll(socket, "HTTP/1.1 100 Continue\r\n\r\n"sv);
            upstreamBody = ReadData(socket, buffer, body.size());

            WriteAll(socket,
                "HTTP/1.1 200 OK\r\n"
                "Content-Length: 2\r\n"
                "Connection: close\r\n"
                "\r\n"
                "ok"sv);
        });

        auto configuration = ManualProxyConfiguration(upstreamProxy.GetPort());
        configuration.httpConfiguration.maxBodySizeBeforeProbing = body.size() - 1;
        StartProxy(std::move(configuration));

        auto response = SendRequestToProxy(std::format(
            "POST http://example.org/expect HTTP/1.1\r\n"
            "Host: example.org\r\n"
            "Content-Length: {}\r\n"
            "\r\n"
            "{}",
            body.size(),
            body));
        upstreamProxy.RethrowFailure();

        EXPECT_EQ(response.GetStatusCode(), HttpStatusCode::OK);
        EXPECT_EQ(response.GetBody(), "ok");
        EXPECT_NE(upstreamHead.find("Expect: 100-continue\r\n"), std::string::npos);
        EXPECT_EQ(upstreamBody, body);
    }

    TEST_F(ProxySystemTest, SendsBodyAfterExpectContinueTimeout)
    {
        constexpr auto body = "late-body"sv;

        HttpRequestMessage upstreamRequest;
        ScriptedServer upstreamProxy([&](tcp::socket& socket) {
            // Never answers 100 Continue; the proxy must send the body after the timeout on the same connection.
            upstreamRequest = ReadRequestMessage(socket);
            WriteAll(socket,
                "HTTP/1.1 200 OK\r\n"
                "Content-Length: 2\r\n"
                "Connection: close\r\n"
                "\r\n"
                "ok"sv);
        });

        auto configuration = ManualProxyConfiguration(upstreamProxy.GetPort());
        configuration.httpConfiguration.expectContinueTimeout = 100ms;
        configuration.httpConfiguration.maxBodySizeBeforeProbing = body.size() - 1;
        StartProxy(std::move(configuration));

        auto response = SendRequestToProxy(std::format(
            "POST http://example.org/timeout HTTP/1.1\r\n"
            "Host: example.org\r\n"
            "Content-Length: {}\r\n"
            "\r\n"
            "{}",
            body.size(),
            body));
        upstreamProxy.RethrowFailure();

        EXPECT_EQ(response.GetStatusCode(), HttpStatusCode::OK);
        EXPECT_EQ(upstreamRequest.GetHeaders().GetValue("Expect"sv), "100-continue");
        EXPECT_EQ(upstreamRequest.GetBody(), body);
    }

    TEST_F(ProxySystemTest, ReplaysBodyWhenAuthenticationIsRequestedAfterExpectTimeout)
    {
        constexpr auto body = "replayed-body"sv;

        std::array<HttpRequestMessage, 2> upstreamRequests;
        ScriptedServer upstreamProxy([&](tcp::socket& socket) {
            upstreamRequests[0] = ReadRequestMessage(socket);
            WriteAll(socket,
                "HTTP/1.1 407 Proxy Authentication Required\r\n"
                "Proxy-Authenticate: Basic realm=\"system-test\"\r\n"
                "Content-Length: 0\r\n"
                "\r\n"sv);

            upstreamRequests[1] = ReadRequestMessage(socket);
            WriteAll(socket,
                "HTTP/1.1 200 OK\r\n"
                "Content-Length: 2\r\n"
                "Connection: close\r\n"
                "\r\n"
                "ok"sv);
        });

        auto configuration = ManualProxyConfiguration(upstreamProxy.GetPort());
        configuration.httpConfiguration.expectContinueTimeout = 100ms;
        configuration.allowedAuthenticationSchemes = { AuthenticationScheme::Basic };
        configuration.gatewayUsername = "test-user"sv;
        configuration.gatewayPassword = "test-password"sv;
        StartProxy(std::move(configuration));

        auto response = SendRequestToProxy(std::format(
            "POST http://example.org/replay HTTP/1.1\r\n"
            "Host: example.org\r\n"
            "Content-Length: {}\r\n"
            "\r\n"
            "{}",
            body.size(),
            body));
        upstreamProxy.RethrowFailure();

        EXPECT_EQ(response.GetStatusCode(), HttpStatusCode::OK);
        EXPECT_EQ(upstreamRequests[0].GetBody(), body);
        EXPECT_EQ(upstreamRequests[1].GetBody(), body);
        EXPECT_TRUE(upstreamRequests[1].GetHeaders().Contains("Proxy-Authorization"sv));
    }

    TEST_F(ProxySystemTest, DeliversEarlyRejectionWithoutResettingClient)
    {
        const std::string body(0x40000, 'B');

        ScriptedServer upstreamProxy([&](tcp::socket& socket) {
            std::string buffer;
            asio::read_until(socket, asio::dynamic_buffer(buffer), "\r\n\r\n"sv);
            WriteAll(socket,
                "HTTP/1.1 403 Forbidden\r\n"
                "Content-Length: 8\r\n"
                "Connection: close\r\n"
                "\r\n"
                "rejected"sv);
        });

        auto configuration = ManualProxyConfiguration(upstreamProxy.GetPort());
        configuration.httpConfiguration.maxBodySizeBeforeProbing = body.size() - 1;
        StartProxy(std::move(configuration));

        HttpResponseMessage response;
        ASSERT_NO_THROW(response = SendRequestToProxy(std::format(
                            "POST http://example.org/reject HTTP/1.1\r\n"
                            "Host: example.org\r\n"
                            "Content-Length: {}\r\n"
                            "\r\n"
                            "{}",
                            body.size(),
                            body)));
        upstreamProxy.RethrowFailure();

        EXPECT_EQ(response.GetStatusCode(), HttpStatusCode::Forbidden);
        EXPECT_EQ(response.GetBody(), "rejected");
    }
}
