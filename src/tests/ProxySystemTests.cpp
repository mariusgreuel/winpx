//
// ProxySystemTests.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include <common/pch.h>

#include <common/Proxy.h>
#include <common/Tools.h>
#include <win32/Base64.h>

#include <gtest/gtest.h>

namespace SystemTests
{
    using namespace std::literals::string_view_literals;
    using namespace std::chrono_literals;
    using namespace win32;
    using namespace winpx;
    using asio::ip::tcp;

    static constexpr size_t largeChunkSize = 0x11000;

    static std::string MakeChunkedBody(const std::vector<std::string_view>& chunks)
    {
        std::string result;

        for (const auto& chunk : chunks)
        {
            result += std::format("{:X}\r\n{}\r\n", chunk.size(), chunk);
        }

        result += "0\r\n\r\n";
        return result;
    }

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

        auto token = Trim(authorization->substr(scheme.size()), ' ');
        return Base64::Decode(token);
    }

    static size_t GetContentLength(std::string_view head)
    {
        constexpr std::string_view headerName = "Content-Length:"sv;
        auto position = head.find(headerName);
        if (position == std::string_view::npos)
            return 0;

        position += headerName.size();
        auto end = head.find("\r\n", position);
        return static_cast<size_t>(std::stoull(std::string(head.substr(position, end - position))));
    }

    static std::tuple<std::string, size_t> ReadMessage(tcp::socket& socket)
    {
        std::string buffer;
        auto headSize = asio::read_until(socket, asio::dynamic_buffer(buffer), "\r\n\r\n"sv);
        auto contentLength = GetContentLength(std::string_view(buffer).substr(0, headSize));

        if (buffer.size() < headSize + contentLength)
        {
            asio::read(socket, asio::dynamic_buffer(buffer), asio::transfer_exactly(headSize + contentLength - buffer.size()));
        }

        return { buffer, headSize };
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

        explicit ScriptedServer(Handler handler) :
            m_acceptor(m_ioContext, tcp::endpoint(asio::ip::address_v4::loopback(), 0)),
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
                tcp::socket socket(m_ioContext);
                m_acceptor.accept(socket);
                m_handler(socket);
            }
            catch (...)
            {
                m_failure = std::current_exception();
            }
        }

        asio::io_context m_ioContext;
        tcp::acceptor m_acceptor;
        Handler m_handler;
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

        ProxyConfiguration ManualConfiguration(uint16_t upstreamPort) const
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

        HttpResponseMessage SendRequest(std::string_view request) const
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
        ScriptedServer origin([&](tcp::socket& socket) {
            originRequest = ReadRequestMessage(socket);
            WriteAll(socket,
                "HTTP/1.1 200 OK\r\n"
                "Content-Length: 8\r\n"
                "Connection: close\r\n"
                "\r\n"
                "get-body"sv);
        });

        StartProxy(DirectConfiguration());

        auto response = SendRequest(std::format(
            "GET http://127.0.0.1:{0}/resource HTTP/1.1\r\n"
            "Host: 127.0.0.1:{0}\r\n"
            "Proxy-Connection: keep-alive\r\n"
            "Proxy-Authorization: Basic ignored\r\n"
            "X-Test: direct-get\r\n"
            "\r\n",
            origin.GetPort()));
        origin.RethrowFailure();

        EXPECT_EQ(response.GetStatusCode(), HttpStatusCode::OK);
        EXPECT_EQ(response.GetBody(), "get-body");
        EXPECT_EQ(originRequest.GetMethod(), "GET");
        EXPECT_EQ(originRequest.GetRequestUri(), "/resource");
        EXPECT_EQ(originRequest.GetHeaders().GetValue("X-Test"sv), "direct-get");
        EXPECT_EQ(originRequest.GetHeaders().GetValue("Connection"sv), "close");
        EXPECT_EQ(originRequest.GetHeaders().GetValue("Via"sv), std::format("1.1 winpx:{}", m_configuration.port));
        EXPECT_FALSE(originRequest.GetHeaders().Contains("Proxy-Connection"sv));
        EXPECT_FALSE(originRequest.GetHeaders().Contains("Proxy-Authorization"sv));
        EXPECT_TRUE(originRequest.GetBody().empty());
    }

    TEST_F(ProxySystemTest, DirectPostRequest)
    {
        HttpRequestMessage originRequest;
        ScriptedServer origin([&](tcp::socket& socket) {
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
        auto response = SendRequest(std::format(
            "POST http://127.0.0.1:{}/submit HTTP/1.1\r\n"
            "Host: 127.0.0.1:{}\r\n"
            "Content-Type: application/x-www-form-urlencoded\r\n"
            "Content-Length: {}\r\n"
            "\r\n"
            "{}",
            origin.GetPort(), origin.GetPort(), body.size(), body));
        origin.RethrowFailure();

        EXPECT_EQ(response.GetStatusCode(), HttpStatusCode::Created);
        EXPECT_EQ(response.GetBody(), "post-body");
        EXPECT_EQ(originRequest.GetMethod(), "POST");
        EXPECT_EQ(originRequest.GetRequestUri(), "/submit");
        EXPECT_EQ(originRequest.GetHeaders().GetValue("Content-Type"sv), "application/x-www-form-urlencoded");
        EXPECT_EQ(originRequest.GetHeaders().GetValue("Connection"sv), "close");
        EXPECT_EQ(originRequest.GetBody(), body);
    }

    TEST_F(ProxySystemTest, DirectHeadRequest)
    {
        HttpRequestMessage originRequest;
        ScriptedServer origin([&](tcp::socket& socket) {
            originRequest = ReadRequestMessage(socket);
            WriteAll(socket,
                "HTTP/1.1 200 OK\r\n"
                "Content-Length: 123\r\n"
                "Connection: close\r\n"
                "\r\n"sv);
        });

        StartProxy(DirectConfiguration());

        auto response = SendRequest(std::format(
            "HEAD http://127.0.0.1:{0}/metadata HTTP/1.1\r\n"
            "Host: 127.0.0.1:{0}\r\n"
            "\r\n",
            origin.GetPort()));
        origin.RethrowFailure();

        EXPECT_EQ(response.GetStatusCode(), HttpStatusCode::OK);
        EXPECT_EQ(response.GetHeaders().GetValue("Content-Length"sv), "123");
        EXPECT_EQ(response.GetHeaders().GetValue("Connection"sv), "close");
        EXPECT_TRUE(response.GetBody().empty());
        EXPECT_EQ(originRequest.GetMethod(), "HEAD");
        EXPECT_EQ(originRequest.GetRequestUri(), "/metadata");
    }

    TEST_F(ProxySystemTest, ProxyGetRequest)
    {
        HttpRequestMessage upstreamRequest;
        ScriptedServer upstream([&](tcp::socket& socket) {
            upstreamRequest = ReadRequestMessage(socket);
            WriteAll(socket,
                "HTTP/1.1 200 OK\r\n"
                "Content-Length: 8\r\n"
                "Connection: close\r\n"
                "\r\n"
                "get-body"sv);
        });

        StartProxy(ManualConfiguration(upstream.GetPort()));

        auto response = SendRequest(
            "GET http://example.org/resource HTTP/1.1\r\n"
            "Host: example.org\r\n"
            "Proxy-Connection: keep-alive\r\n"
            "X-Test: proxy-get\r\n"
            "\r\n"sv);
        upstream.RethrowFailure();

        EXPECT_EQ(response.GetStatusCode(), HttpStatusCode::OK);
        EXPECT_EQ(response.GetBody(), "get-body");
        EXPECT_EQ(upstreamRequest.GetMethod(), "GET");
        EXPECT_EQ(upstreamRequest.GetRequestUri(), "http://example.org/resource");
        EXPECT_EQ(upstreamRequest.GetHeaders().GetValue("X-Test"sv), "proxy-get");
        EXPECT_EQ(upstreamRequest.GetHeaders().GetValue("Connection"sv), "keep-alive");
        EXPECT_EQ(upstreamRequest.GetHeaders().GetValue("Via"sv), std::format("1.1 winpx:{}", m_configuration.port));
        EXPECT_FALSE(upstreamRequest.GetHeaders().Contains("Proxy-Connection"sv));
        EXPECT_FALSE(upstreamRequest.GetHeaders().Contains("Proxy-Authorization"sv));
        EXPECT_TRUE(upstreamRequest.GetBody().empty());
    }

    TEST_F(ProxySystemTest, ProxyPostRequest)
    {
        HttpRequestMessage upstreamRequest;
        ScriptedServer upstream([&](tcp::socket& socket) {
            upstreamRequest = ReadRequestMessage(socket);
            WriteAll(socket,
                "HTTP/1.1 201 Created\r\n"
                "Content-Length: 9\r\n"
                "Connection: close\r\n"
                "\r\n"
                "post-body"sv);
        });

        StartProxy(ManualConfiguration(upstream.GetPort()));

        constexpr auto body = "name=winpx&mode=system-test"sv;
        auto response = SendRequest(std::format(
            "POST http://example.org/submit HTTP/1.1\r\n"
            "Host: example.org\r\n"
            "Content-Type: application/x-www-form-urlencoded\r\n"
            "Content-Length: {}\r\n"
            "\r\n"
            "{}",
            body.size(), body));
        upstream.RethrowFailure();

        EXPECT_EQ(response.GetStatusCode(), HttpStatusCode::Created);
        EXPECT_EQ(response.GetBody(), "post-body");
        EXPECT_EQ(upstreamRequest.GetMethod(), "POST");
        EXPECT_EQ(upstreamRequest.GetRequestUri(), "http://example.org/submit");
        EXPECT_EQ(upstreamRequest.GetHeaders().GetValue("Content-Type"sv), "application/x-www-form-urlencoded");
        EXPECT_EQ(upstreamRequest.GetHeaders().GetValue("Connection"sv), "keep-alive");
        EXPECT_EQ(upstreamRequest.GetHeaders().GetValue("Via"sv), std::format("1.1 winpx:{}", m_configuration.port));
        EXPECT_FALSE(upstreamRequest.GetHeaders().Contains("Proxy-Connection"sv));
        EXPECT_FALSE(upstreamRequest.GetHeaders().Contains("Proxy-Authorization"sv));
        EXPECT_EQ(upstreamRequest.GetBody(), body);
    }

    TEST_F(ProxySystemTest, ProxyHeadRequest)
    {
        HttpRequestMessage upstreamRequest;
        ScriptedServer upstream([&](tcp::socket& socket) {
            upstreamRequest = ReadRequestMessage(socket);
            WriteAll(socket,
                "HTTP/1.1 200 OK\r\n"
                "Content-Length: 123\r\n"
                "Connection: close\r\n"
                "\r\n"sv);
        });

        StartProxy(ManualConfiguration(upstream.GetPort()));

        auto response = SendRequest(std::format(
            "HEAD http://example.org/metadata HTTP/1.1\r\n"
            "Host: example.org\r\n"
            "\r\n"));
        upstream.RethrowFailure();

        EXPECT_EQ(response.GetStatusCode(), HttpStatusCode::OK);
        EXPECT_EQ(response.GetHeaders().GetValue("Content-Length"sv), "123");
        EXPECT_EQ(response.GetHeaders().GetValue("Connection"sv), "close");
        EXPECT_TRUE(response.GetBody().empty());
        EXPECT_EQ(upstreamRequest.GetMethod(), "HEAD");
        EXPECT_EQ(upstreamRequest.GetRequestUri(), "http://example.org/metadata");
        EXPECT_EQ(upstreamRequest.GetHeaders().GetValue("Connection"sv), "keep-alive");
        EXPECT_EQ(upstreamRequest.GetHeaders().GetValue("Via"sv), std::format("1.1 winpx:{}", m_configuration.port));
        EXPECT_FALSE(upstreamRequest.GetHeaders().Contains("Proxy-Connection"sv));
        EXPECT_FALSE(upstreamRequest.GetHeaders().Contains("Proxy-Authorization"sv));
        EXPECT_TRUE(upstreamRequest.GetBody().empty());
    }

    TEST_F(ProxySystemTest, DecodesChunkedRequestBody)
    {
        const std::string largeChunk(largeChunkSize, 'X');
        const std::string chunkedBody = MakeChunkedBody({ "hello"sv, largeChunk, "world"sv });
        const std::string decodedBody = "hello" + largeChunk + "world";

        HttpRequestMessage originRequest;
        ScriptedServer origin([&](tcp::socket& socket) {
            originRequest = ReadRequestMessage(socket);
            WriteAll(socket,
                "HTTP/1.1 200 OK\r\n"
                "Content-Length: 15\r\n"
                "Connection: close\r\n"
                "\r\n"
                "chunked-request"sv);
        });

        StartProxy(DirectConfiguration());

        auto response = SendRequest(std::format(
            "POST http://127.0.0.1:{0}/upload HTTP/1.1\r\n"
            "Host: 127.0.0.1:{0}\r\n"
            "Transfer-Encoding: chunked\r\n"
            "\r\n"
            "{1}",
            origin.GetPort(),
            chunkedBody));
        origin.RethrowFailure();

        EXPECT_EQ(response.GetStatusCode(), HttpStatusCode::OK);
        EXPECT_EQ(response.GetBody(), "chunked-request");
        EXPECT_EQ(originRequest.GetMethod(), "POST");
        EXPECT_EQ(originRequest.GetRequestUri(), "/upload");
        EXPECT_FALSE(originRequest.GetHeaders().Contains("Transfer-Encoding"sv));
        EXPECT_EQ(originRequest.GetHeaders().GetValue("Content-Length"sv), std::to_string(decodedBody.size()));
        EXPECT_EQ(originRequest.GetBody(), decodedBody);
    }

    TEST_F(ProxySystemTest, ForwardsChunkedResponseBody)
    {
        const std::string largeChunk(largeChunkSize, 'X');
        const std::string chunkedBody = MakeChunkedBody({ "hello"sv, largeChunk, "world"sv });

        HttpRequestMessage originRequest;
        ScriptedServer origin([&](tcp::socket& socket) {
            originRequest = ReadRequestMessage(socket);
            WriteAll(socket,
                "HTTP/1.1 200 OK\r\n"
                "Transfer-Encoding: chunked\r\n"
                "\r\n"sv);
            WriteAll(socket, chunkedBody);
        });

        StartProxy(DirectConfiguration());

        auto response = SendRequest(std::format(
            "GET http://127.0.0.1:{0}/stream HTTP/1.1\r\n"
            "Host: 127.0.0.1:{0}\r\n"
            "\r\n"sv,
            origin.GetPort()));
        origin.RethrowFailure();

        EXPECT_EQ(response.GetStatusCode(), HttpStatusCode::OK);
        EXPECT_EQ(response.GetHeaders().GetValue("Transfer-Encoding"sv), "chunked");
        EXPECT_EQ(response.GetBody(), chunkedBody);
        EXPECT_EQ(originRequest.GetMethod(), "GET");
    }

    TEST_F(ProxySystemTest, TunnelsHttpsDirectly)
    {
        constexpr auto payload = "simulated-tls-record"sv;

        ScriptedServer target([&](tcp::socket& socket) {
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
                             target.GetPort()));

        auto response = ReadResponseMessage(client);
        ASSERT_EQ(response.GetStatusCode(), HttpStatusCode::OK);
        ASSERT_EQ(response.GetReasonPhrase(), "Connection Established");

        WriteAll(client, payload);
        std::string echoed(payload.size(), '\0');
        asio::read(client, asio::buffer(echoed));
        EXPECT_EQ(payload, echoed);
        target.RethrowFailure();
    }

    TEST_F(ProxySystemTest, TunnelsHttpsThroughUpstreamProxy)
    {
        constexpr auto payload = "simulated-tls-record"sv;

        HttpRequestMessage connectRequest;
        ScriptedServer upstream([&](tcp::socket& socket) {
            connectRequest = ReadRequestMessage(socket);
            WriteAll(socket, "HTTP/1.1 200 Connection Established\r\n\r\n"sv);

            std::string received(payload.size(), '\0');
            asio::read(socket, asio::buffer(received));
            ASSERT_EQ(payload, received);
            WriteAll(socket, received);
        });

        StartProxy(ManualConfiguration(upstream.GetPort()));
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
        upstream.RethrowFailure();

        EXPECT_EQ(connectRequest.GetMethod(), "CONNECT");
        EXPECT_EQ(connectRequest.GetRequestUri(), "secure.example.test:443");
        EXPECT_EQ(connectRequest.GetHeaders().GetValue("Host"sv), "secure.example.test:443");
    }

    TEST_F(ProxySystemTest, UpstreamProxyBasicAuthentication)
    {
        std::array<HttpRequestMessage, 2> upstreamRequests;
        ScriptedServer upstream([&](tcp::socket& socket) {
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

        auto configuration = ManualConfiguration(upstream.GetPort());
        configuration.allowedAuthenticationSchemes = { AuthenticationScheme::Basic };
        configuration.gatewayUsername = "test-user"sv;
        configuration.gatewayPassword = "test-password"sv;
        StartProxy(std::move(configuration));

        auto response = SendRequest(
            "GET http://example.org/basic HTTP/1.1\r\n"
            "Host: example.org\r\n"
            "\r\n"sv);
        upstream.RethrowFailure();

        EXPECT_EQ(response.GetStatusCode(), HttpStatusCode::OK);
        EXPECT_EQ(response.GetBody(), "basic-passed");
        EXPECT_FALSE(upstreamRequests[0].GetHeaders().Contains("Proxy-Authorization"sv));
        EXPECT_EQ(upstreamRequests[1].GetBody(), upstreamRequests[0].GetBody());
        EXPECT_EQ(upstreamRequests[1].GetMethod(), "GET");
        EXPECT_EQ(upstreamRequests[1].GetRequestUri(), "http://example.org/basic");
        ASSERT_EQ(upstreamRequests[1].GetHeaders().GetValue("Proxy-Authorization"sv), "Basic " + Base64::Encode("test-user:test-password"sv));
    }

    TEST_F(ProxySystemTest, StopsBasicHandshakeAfterCredentialsAreRejected)
    {
        std::array<HttpRequestMessage, 2> upstreamRequests;
        ScriptedServer upstream([&](tcp::socket& socket) {
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

        auto configuration = ManualConfiguration(upstream.GetPort());
        configuration.allowedAuthenticationSchemes = { AuthenticationScheme::Basic };
        configuration.gatewayUsername = "wrong-user"sv;
        configuration.gatewayPassword = "wrong-password"sv;
        StartProxy(std::move(configuration));

        auto response = SendRequest(
            "GET http://example.org/basic HTTP/1.1\r\n"
            "Host: example.org\r\n"
            "\r\n"sv);
        upstream.RethrowFailure();

        EXPECT_EQ(response.GetStatusCode(), HttpStatusCode::ProxyAuthenticationRequired);
        EXPECT_FALSE(upstreamRequests[0].GetHeaders().Contains("Proxy-Authorization"sv));
        ASSERT_EQ(upstreamRequests[1].GetHeaders().GetValue("Proxy-Authorization"sv), "Basic " + Base64::Encode("wrong-user:wrong-password"sv));
    }

    TEST_F(ProxySystemTest, ReturnsChallengeWhenUpstreamOffersNoAllowedAuthenticationScheme)
    {
        HttpRequestMessage upstreamRequest;
        ScriptedServer upstream([&](tcp::socket& socket) {
            upstreamRequest = ReadRequestMessage(socket);
            WriteAll(socket,
                "HTTP/1.1 407 Proxy Authentication Required\r\n"
                "Proxy-Authenticate: Basic realm=\"system-test\"\r\n"
                "Content-Length: 0\r\n"
                "Connection: close\r\n"
                "\r\n"sv);
        });

        auto configuration = ManualConfiguration(upstream.GetPort());
        configuration.allowedAuthenticationSchemes = { AuthenticationScheme::Negotiate };
        StartProxy(std::move(configuration));

        auto response = SendRequest(
            "GET http://example.org/rejected HTTP/1.1\r\n"
            "Host: example.org\r\n"
            "\r\n"sv);
        upstream.RethrowFailure();

        EXPECT_EQ(response.GetStatusCode(), HttpStatusCode::ProxyAuthenticationRequired);
        EXPECT_EQ(response.GetHeaders().GetValue("Proxy-Authenticate"sv), "Basic realm=\"system-test\"");
        EXPECT_FALSE(upstreamRequest.GetHeaders().Contains("Proxy-Authorization"sv));
    }

    TEST_F(ProxySystemTest, CompletesUpstreamNegotiateHandshake)
    {
        std::vector<HttpRequestMessage> upstreamRequests;
        std::vector<SECURITY_STATUS> serverStatuses;
        std::optional<size_t> prematureCloseRound;

        ScriptedServer upstream([&](tcp::socket& socket) {
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

        auto configuration = ManualConfiguration(upstream.GetPort());
        configuration.allowedAuthenticationSchemes = { AuthenticationScheme::Negotiate, AuthenticationScheme::Ntlm };
        StartProxy(std::move(configuration));

        auto response = SendRequest(
            "GET http://example.org/negotiate HTTP/1.1\r\n"
            "Host: example.org\r\n"
            "\r\n"sv);
        upstream.RethrowFailure();

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
}
