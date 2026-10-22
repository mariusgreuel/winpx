//
// HttpTransportTests.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include <common/pch.h>

#include "TestHelpers.h"
#include <common/HttpTransport.h>

#include <gtest/gtest.h>

namespace UnitTests
{
    using namespace winpx;
    using namespace winpx::tests;
    using namespace std::literals::string_view_literals;
    using asio::ip::tcp;

    class HttpTransportTests : public testing::Test
    {
    protected:
        struct SocketPair
        {
            tcp::socket local;
            tcp::socket remote;
        };

        void SetUp() override
        {
            m_configuration.resolveTimeout = std::chrono::milliseconds(100);
            m_configuration.connectTimeout = std::chrono::milliseconds(100);
            m_configuration.readTimeout = std::chrono::milliseconds(100);
            m_configuration.writeTimeout = std::chrono::milliseconds(100);
            m_configuration.drainTimeout = std::chrono::milliseconds(100);
        }

        SocketPair CreateSocketPair()
        {
            tcp::acceptor acceptor(m_ioContext, tcp::endpoint(asio::ip::address_v4::loopback(), 0));
            tcp::socket remote(m_ioContext);
            remote.connect(acceptor.local_endpoint());
            tcp::socket local = acceptor.accept();
            return { std::move(local), std::move(remote) };
        }

        std::tuple<SocketPair, SocketPair> CreateTwoSocketPairs()
        {
            return { CreateSocketPair(), CreateSocketPair() };
        }

        template<typename T>
        T Run(asio::awaitable<T> awaitable)
        {
            auto future = asio::co_spawn(m_ioContext, std::move(awaitable), asio::use_future);
            m_ioContext.restart();
            m_ioContext.run();
            return future.get();
        }

        static void WriteAndShutdown(tcp::socket& socket, std::string_view data)
        {
            if (!data.empty())
            {
                asio::write(socket, asio::buffer(data));
            }

            socket.shutdown(asio::socket_base::shutdown_send);
        }

        static std::string ReadAll(tcp::socket& socket)
        {
            std::string result;
            asio::error_code ec;
            asio::read(socket, asio::dynamic_buffer(result), ec);
            if (ec && ec != asio::error::eof)
            {
                throw std::system_error(ec);
            }

            return result;
        }

        static std::string ReadAllAfterShutdown(tcp::socket& writer, tcp::socket& reader)
        {
            writer.shutdown(asio::socket_base::shutdown_send);
            return ReadAll(reader);
        }

        asio::io_context m_ioContext;
        HttpConfiguration m_configuration;
        HttpTransport m_transport{ m_configuration };
    };

    TEST_F(HttpTransportTests, ParseChunkSize_ParsesHex)
    {
        m_configuration.maxChunkSize = 0xFFFFFFFFFFFFu;
        EXPECT_EQ(m_transport.ParseChunkSize("0\r\n"sv), 0u);
        EXPECT_EQ(m_transport.ParseChunkSize("123\r\n"sv), 0x123u);
        EXPECT_EQ(m_transport.ParseChunkSize("FF12345678\r\n"sv), 0xFF12345678u);
    }

    TEST_F(HttpTransportTests, ParseChunkSize_IgnoresExtensions)
    {
        EXPECT_EQ(m_transport.ParseChunkSize("a;name=value\r\n"sv), 0xAu);
        EXPECT_EQ(m_transport.ParseChunkSize("ff  ;name=value\r\n"sv), 0xFFu);
    }

    TEST_F(HttpTransportTests, ParseChunkSize_ThrowsOnInvalidSize)
    {
        EXPECT_THROW(m_transport.ParseChunkSize("xyz\r\n"sv), std::runtime_error);
    }

    TEST_F(HttpTransportTests, ParseChunkSize_ThrowsWhenExceedingMaxChunkSize)
    {
        m_configuration.maxChunkSize = 0x10;
        EXPECT_EQ(m_transport.ParseChunkSize("10\r\n"sv), 0x10u);
        EXPECT_THROW(m_transport.ParseChunkSize("11\r\n"sv), std::runtime_error);
    }

    TEST_F(HttpTransportTests, ConnectTo_ConnectsToListeningEndpoint)
    {
        tcp::acceptor acceptor(m_ioContext, tcp::endpoint(asio::ip::address_v4::loopback(), 0));
        uint16_t port = acceptor.local_endpoint().port();

        std::optional<tcp::socket> socket;
        Run([&]() -> asio::awaitable<void> {
            socket.emplace(co_await m_transport.ConnectTo("127.0.0.1", port));
        }());

        ASSERT_TRUE(socket.has_value());
        EXPECT_TRUE(socket->is_open());
        EXPECT_EQ(socket->remote_endpoint().port(), port);
    }

    TEST_F(HttpTransportTests, ConnectTo_ThrowsWhenConnectionRefused)
    {
        uint16_t port;
        {
            tcp::acceptor acceptor(m_ioContext, tcp::endpoint(asio::ip::address_v4::loopback(), 0));
            port = acceptor.local_endpoint().port();
        }

        EXPECT_THROW(Run([&]() -> asio::awaitable<void> {
            co_await m_transport.ConnectTo("127.0.0.1", port);
        }()),
            std::system_error);
    }

    TEST_F(HttpTransportTests, DrainAndClose_DrainsPendingDataAndClosesSocket)
    {
        auto pair = CreateSocketPair();

        WriteAndShutdown(pair.remote, "pending request data");
        Run(m_transport.DrainAndClose(pair.local));
        EXPECT_FALSE(pair.local.is_open());
        EXPECT_EQ(ReadAll(pair.remote), "");
    }

    TEST_F(HttpTransportTests, DrainAndClose_ClosesAfterDrainTimeout)
    {
        auto pair = CreateSocketPair();

        m_configuration.drainTimeout = std::chrono::milliseconds(100);
        Run(m_transport.DrainAndClose(pair.local));
        EXPECT_FALSE(pair.local.is_open());
    }

    TEST_F(HttpTransportTests, ReadData_ZeroLengthReturnsEmptyBuffer)
    {
        auto pair = CreateSocketPair();

        std::string buffer = "abc";
        EXPECT_TRUE(Run(m_transport.ReadData(pair.local, buffer, 0)).empty());
    }

    TEST_F(HttpTransportTests, ReadData_ReadsExactLength)
    {
        auto pair = CreateSocketPair();
        asio::write(pair.remote, asio::buffer("hello world"sv));

        std::string buffer;
        std::string data = Run(m_transport.ReadData(pair.local, buffer, 5));
        EXPECT_EQ(data, "hello");
    }

    TEST_F(HttpTransportTests, ReadData_UsesLeftoverBuffer)
    {
        auto pair = CreateSocketPair();
        asio::write(pair.remote, asio::buffer("def"sv));

        std::string buffer = "abc";
        std::string data = Run(m_transport.ReadData(pair.local, buffer, 6));
        EXPECT_EQ(data, "abcdef");
        EXPECT_TRUE(buffer.empty());
    }

    TEST_F(HttpTransportTests, ReadData_ThrowsOnEof)
    {
        auto pair = CreateSocketPair();
        WriteAndShutdown(pair.remote, "abc");

        std::string buffer;
        EXPECT_THROW(Run(m_transport.ReadData(pair.local, buffer, 4)), std::system_error);
    }

    TEST_F(HttpTransportTests, ReadData_ReturnsPartialDataOnEof)
    {
        auto pair = CreateSocketPair();
        WriteAndShutdown(pair.remote, "abc");

        std::string buffer;
        EXPECT_EQ(Run(m_transport.ReadData(pair.local, buffer, HttpTransport::untilEof)), "abc");
    }

    TEST_F(HttpTransportTests, ReadChunkedData_DecodesChunks)
    {
        auto pair = CreateSocketPair();
        asio::write(pair.remote, asio::buffer(MakeChunkedBody({ "hello"sv, " world"sv })));

        std::string buffer;
        EXPECT_EQ(Run(m_transport.ReadChunkedData(pair.local, buffer)), "hello world");
    }

    TEST_F(HttpTransportTests, ReadChunkedData_SkipsTrailers)
    {
        auto pair = CreateSocketPair();
        asio::write(pair.remote, asio::buffer(MakeChunkedBody({ "abc"sv }, { "X-Trailer: 1"sv })));

        std::string buffer;
        EXPECT_EQ(Run(m_transport.ReadChunkedData(pair.local, buffer)), "abc");
    }

    TEST_F(HttpTransportTests, ReadChunkedData_ThrowsOnMissingCr)
    {
        auto pair = CreateSocketPair();
        asio::write(pair.remote, asio::buffer("3\nabc\r\n0\r\n\r\n"sv));

        std::string buffer;
        EXPECT_THROW(Run(m_transport.ReadChunkedData(pair.local, buffer)), std::runtime_error);
    }

    TEST_F(HttpTransportTests, ReadChunkedData_ThrowsOnMissingChunkTerminator)
    {
        auto pair = CreateSocketPair();
        asio::write(pair.remote, asio::buffer("3\r\nabcX\r\n0\r\n\r\n"sv));

        std::string buffer;
        EXPECT_THROW(Run(m_transport.ReadChunkedData(pair.local, buffer)), std::runtime_error);
    }

    TEST_F(HttpTransportTests, ReadBody_ContentLength)
    {
        auto pair = CreateSocketPair();
        asio::write(pair.remote, asio::buffer("abcdef"sv));

        std::string buffer;
        HttpMessageFraming framing{ false, 4 };
        EXPECT_EQ(Run(m_transport.ReadBody(pair.local, buffer, framing)), "abcd");
    }

    TEST_F(HttpTransportTests, ReadBody_Chunked)
    {
        auto pair = CreateSocketPair();
        asio::write(pair.remote, asio::buffer("3\r\nabc\r\n0\r\n\r\n"sv));

        std::string buffer;
        HttpMessageFraming framing{ true };
        EXPECT_EQ(Run(m_transport.ReadBody(pair.local, buffer, framing)), "abc");
    }

    TEST_F(HttpTransportTests, ReadBody_NoFramingReturnsEmpty)
    {
        auto pair = CreateSocketPair();

        std::string buffer;
        HttpMessageFraming framing{};
        EXPECT_EQ(Run(m_transport.ReadBody(pair.local, buffer, framing)), "");
    }

    TEST_F(HttpTransportTests, WriteData_WritesAllBytes)
    {
        auto pair = CreateSocketPair();

        auto buffer = "test"sv;
        size_t bytesWritten = Run(m_transport.WriteData(pair.local, buffer));
        EXPECT_EQ(bytesWritten, buffer.size());
        EXPECT_EQ(ReadAllAfterShutdown(pair.local, pair.remote), buffer);
    }

    TEST_F(HttpTransportTests, WriteData_EmptyBufferReturnsZero)
    {
        auto pair = CreateSocketPair();

        EXPECT_EQ(Run(m_transport.WriteData(pair.local, ""sv)), 0u);
        EXPECT_EQ(ReadAllAfterShutdown(pair.local, pair.remote), "");
    }

    TEST_F(HttpTransportTests, WriteResponse_DoesNotOverwriteContentLength)
    {
        auto pair = CreateSocketPair();

        HttpResponseMessage response(HttpStatusCode::OK);
        response.GetHeaders().Add("Content-Length", "3");
        response.SetBody("abcdef");
        Run(m_transport.WriteResponse(pair.local, std::move(response)));

        std::string received = ReadAllAfterShutdown(pair.local, pair.remote);
        EXPECT_NE(received.find("Content-Length: 3\r\n"), std::string::npos);
    }

    TEST_F(HttpTransportTests, StreamData_ForwardsExactLength)
    {
        auto [source, destination] = CreateTwoSocketPairs();
        asio::write(source.remote, asio::buffer("hello world"sv));

        std::string buffer;
        size_t bytes = Run(m_transport.StreamData(source.local, destination.local, buffer, 5));
        EXPECT_EQ(bytes, 5u);
        EXPECT_EQ(ReadAllAfterShutdown(destination.local, destination.remote), "hello");
    }

    TEST_F(HttpTransportTests, StreamData_UsesLeftoverBufferAndCapturesReplay)
    {
        auto [source, destination] = CreateTwoSocketPairs();
        asio::write(source.remote, asio::buffer("def"sv));

        std::string buffer = "abc";
        HttpReplayBuffer replay;
        size_t bytes = Run(m_transport.StreamData(source.local, destination.local, buffer, 6, &replay));
        EXPECT_EQ(bytes, 6u);
        EXPECT_EQ(replay.GetData(), "abcdef");
        EXPECT_EQ(ReadAllAfterShutdown(destination.local, destination.remote), "abcdef");
    }

    TEST_F(HttpTransportTests, StreamData_ZeroLengthReturnsZero)
    {
        auto [source, destination] = CreateTwoSocketPairs();

        std::string buffer;
        EXPECT_EQ(Run(m_transport.StreamData(source.local, destination.local, buffer, 0)), 0u);
    }

    TEST_F(HttpTransportTests, StreamData_StopsOnEof)
    {
        auto [source, destination] = CreateTwoSocketPairs();
        WriteAndShutdown(source.remote, "abc");

        std::string buffer;
        size_t bytes = Run(m_transport.StreamData(source.local, destination.local, buffer, HttpTransport::untilEof));
        EXPECT_EQ(bytes, 3u);
        EXPECT_EQ(ReadAllAfterShutdown(destination.local, destination.remote), "abc");
    }

    TEST_F(HttpTransportTests, StreamChunkedData_ForwardsRawChunksAndTrailers)
    {
        constexpr auto raw = "5\r\nhello\r\n1;ext=1\r\n!\r\n0\r\nX-Trailer: 1\r\n\r\n"sv;

        auto [source, destination] = CreateTwoSocketPairs();
        asio::write(source.remote, asio::buffer(raw));

        std::string buffer;
        HttpReplayBuffer replay;
        size_t bytes = Run(m_transport.StreamChunkedData(source.local, destination.local, buffer, &replay));
        EXPECT_EQ(bytes, 6u);
        EXPECT_EQ(replay.GetData(), raw);
        EXPECT_EQ(ReadAllAfterShutdown(destination.local, destination.remote), raw);
    }

    TEST_F(HttpTransportTests, StreamChunkedData_ThrowsOnInvalidChunk)
    {
        auto [source, destination] = CreateTwoSocketPairs();
        asio::write(source.remote, asio::buffer("zz\r\n"sv));

        std::string buffer;
        EXPECT_THROW(Run(m_transport.StreamChunkedData(source.local, destination.local, buffer)), std::runtime_error);
    }

    TEST_F(HttpTransportTests, StreamRequestBody_ContentLength)
    {
        auto [client, server] = CreateTwoSocketPairs();
        asio::write(client.remote, asio::buffer("body"sv));

        std::string buffer;
        HttpMessageFraming framing{ .chunked = false, .contentLength = 4 };
        HttpReplayBuffer replay;
        size_t bytes = Run(m_transport.StreamRequestBody(client.local, server.local, buffer, framing, &replay));
        EXPECT_EQ(bytes, 4u);
        EXPECT_EQ(replay.GetData(), "body");
        EXPECT_EQ(ReadAllAfterShutdown(server.local, server.remote), "body");
    }

    TEST_F(HttpTransportTests, StreamRequestBody_Chunked)
    {
        constexpr auto raw = "4\r\nbody\r\n0\r\n\r\n"sv;

        auto [client, server] = CreateTwoSocketPairs();
        asio::write(client.remote, asio::buffer(raw));

        std::string buffer;
        HttpMessageFraming framing{ .chunked = true };
        size_t bytes = Run(m_transport.StreamRequestBody(client.local, server.local, buffer, framing));
        EXPECT_EQ(bytes, 4u);
        EXPECT_EQ(ReadAllAfterShutdown(server.local, server.remote), raw);
    }

    TEST_F(HttpTransportTests, StreamRequestBody_NoFramingStreamsNothing)
    {
        auto [client, server] = CreateTwoSocketPairs();

        std::string buffer;
        HttpMessageFraming framing{};
        EXPECT_EQ(Run(m_transport.StreamRequestBody(client.local, server.local, buffer, framing)), 0u);
    }

    TEST_F(HttpTransportTests, StreamRequestBody_ThrowsOnIncompleteBody)
    {
        auto [client, server] = CreateTwoSocketPairs();

        WriteAndShutdown(client.remote, "abc");

        std::string buffer;
        HttpMessageFraming framing{ .chunked = false, .contentLength = 10 };
        EXPECT_THROW(Run(m_transport.StreamRequestBody(client.local, server.local, buffer, framing)), std::runtime_error);
    }

    TEST_F(HttpTransportTests, StreamResponseBody_ContentLength)
    {
        auto [client, server] = CreateTwoSocketPairs();
        asio::write(server.remote, asio::buffer("response"sv));

        std::string buffer;
        HttpMessageFraming framing{ .chunked = false, .contentLength = 8 };
        size_t bytes = Run(m_transport.StreamResponseBody(server.local, client.local, buffer, framing));
        EXPECT_EQ(bytes, 8u);
        EXPECT_EQ(ReadAllAfterShutdown(client.local, client.remote), "response");
    }

    TEST_F(HttpTransportTests, StreamResponseBody_Chunked)
    {
        auto chunkedBody = MakeChunkedBody({ "abc"sv });

        auto [client, server] = CreateTwoSocketPairs();

        asio::write(server.remote, asio::buffer(chunkedBody));

        std::string buffer;
        HttpMessageFraming framing{ .chunked = true };
        size_t bytes = Run(m_transport.StreamResponseBody(server.local, client.local, buffer, framing));
        EXPECT_EQ(bytes, 3u);
        EXPECT_EQ(ReadAllAfterShutdown(client.local, client.remote), chunkedBody);
    }

    TEST_F(HttpTransportTests, StreamResponseBody_NoFramingStreamsUntilEof)
    {
        auto [client, server] = CreateTwoSocketPairs();

        WriteAndShutdown(server.remote, "until close");

        std::string buffer;
        HttpMessageFraming framing{};
        size_t bytes = Run(m_transport.StreamResponseBody(server.local, client.local, buffer, framing));
        EXPECT_EQ(bytes, 11u);
        EXPECT_EQ(ReadAllAfterShutdown(client.local, client.remote), "until close");
    }

    TEST_F(HttpTransportTests, RelayDataOneWay_RelaysUntilEofAndShutsDownDestination)
    {
        auto [source, destination] = CreateTwoSocketPairs();

        WriteAndShutdown(source.remote, "relayed data");

        size_t bytes = Run(m_transport.RelayDataOneWay(source.local, destination.local));
        EXPECT_EQ(bytes, 12u);
        EXPECT_EQ(ReadAll(destination.remote), "relayed data");
    }

    TEST_F(HttpTransportTests, Tunnel_RelaysBothDirectionsWithLeftovers)
    {
        auto [client, server] = CreateTwoSocketPairs();

        WriteAndShutdown(client.remote, "hello");
        WriteAndShutdown(server.remote, "world!");

        auto result = Run(m_transport.Tunnel(client.local, server.local, "Client:"sv, "Server:"sv));
        EXPECT_EQ(ReadAll(server.remote), "Client:hello");
        EXPECT_EQ(ReadAll(client.remote), "Server:world!");
        EXPECT_EQ(result.serverBytesSent, 12u);
        EXPECT_EQ(result.clientBytesSent, 13u);
        EXPECT_EQ(result.clientBytesReceived, 5u);
        EXPECT_EQ(result.serverBytesReceived, 6u);
    }

    TEST_F(HttpTransportTests, Tunnel_WithoutLeftovers)
    {
        auto [client, server] = CreateTwoSocketPairs();

        WriteAndShutdown(client.remote, "");
        WriteAndShutdown(server.remote, "");

        auto result = Run(m_transport.Tunnel(client.local, server.local, ""sv, ""sv));
        EXPECT_EQ(result.clientBytesSent, 0u);
        EXPECT_EQ(result.clientBytesReceived, 0u);
        EXPECT_EQ(result.serverBytesSent, 0u);
        EXPECT_EQ(result.serverBytesReceived, 0u);
    }
}
