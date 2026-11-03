//
// HttpConnectionPoolTests.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include <common/pch.h>

#include <common/HttpConnectionPool.h>

#include <gtest/gtest.h>

namespace UnitTests
{
    using namespace winpx;
    using namespace std::chrono_literals;
    using asio::ip::tcp;

    class HttpConnectionPoolTest : public testing::Test
    {
    protected:
        struct SocketPair
        {
            tcp::socket client;
            tcp::socket server;
        };

        SocketPair CreateSocketPair()
        {
            tcp::acceptor acceptor(m_ioContext, tcp::endpoint(asio::ip::address_v4::loopback(), 0));
            tcp::socket client(m_ioContext);
            client.connect(acceptor.local_endpoint());
            tcp::socket server(m_ioContext);
            acceptor.accept(server);
            return { std::move(client), std::move(server) };
        }

        asio::io_context m_ioContext;
        HttpConfiguration m_configuration;
    };

    TEST_F(HttpConnectionPoolTest, MakeKey)
    {
        EXPECT_EQ(HttpConnectionPool::MakeKey(true, "my.local", 8080), "direct:my.local:8080");
        EXPECT_EQ(HttpConnectionPool::MakeKey(false, "example.org", 80), "proxy:example.org:80");
    }

    TEST_F(HttpConnectionPoolTest, AcquireFromEmptyPoolReturnsNothing)
    {
        HttpConnectionPool pool(m_configuration);
        EXPECT_FALSE(pool.Acquire("direct:host:80").has_value());
    }

    TEST_F(HttpConnectionPoolTest, AcquireReturnsReleasedConnection)
    {
        HttpConnectionPool pool(m_configuration);
        auto pair = CreateSocketPair();
        auto port = pair.client.local_endpoint().port();

        pool.Release("key", std::move(pair.client));
        auto socket = pool.Acquire("key");

        ASSERT_TRUE(socket.has_value());
        EXPECT_TRUE(socket->is_open());
        EXPECT_EQ(socket->local_endpoint().port(), port);
        EXPECT_FALSE(pool.Acquire("key").has_value());
    }

    TEST_F(HttpConnectionPoolTest, AcquiredConnectionIsBlocking)
    {
        HttpConnectionPool pool(m_configuration);
        auto pair = CreateSocketPair();

        pool.Release("key", std::move(pair.client));
        auto socket = pool.Acquire("key");

        ASSERT_TRUE(socket.has_value());
        EXPECT_FALSE(socket->non_blocking());
    }

    TEST_F(HttpConnectionPoolTest, KeysAreIsolated)
    {
        HttpConnectionPool pool(m_configuration);
        auto pair = CreateSocketPair();

        pool.Release("key1", std::move(pair.client));

        EXPECT_FALSE(pool.Acquire("key2").has_value());
        EXPECT_TRUE(pool.Acquire("key1").has_value());
    }

    TEST_F(HttpConnectionPoolTest, AcquireReturnsMostRecentlyReleasedConnection)
    {
        HttpConnectionPool pool(m_configuration);
        auto pair1 = CreateSocketPair();
        auto pair2 = CreateSocketPair();
        auto port1 = pair1.client.local_endpoint().port();
        auto port2 = pair2.client.local_endpoint().port();

        pool.Release("key", std::move(pair1.client));
        pool.Release("key", std::move(pair2.client));

        auto first = pool.Acquire("key");
        auto second = pool.Acquire("key");
        ASSERT_TRUE(first.has_value());
        ASSERT_TRUE(second.has_value());
        EXPECT_EQ(first->local_endpoint().port(), port2);
        EXPECT_EQ(second->local_endpoint().port(), port1);
    }

    TEST_F(HttpConnectionPoolTest, ReleaseIgnoresClosedSocket)
    {
        HttpConnectionPool pool(m_configuration);
        tcp::socket socket(m_ioContext);

        pool.Release("key", std::move(socket));

        EXPECT_FALSE(pool.Acquire("key").has_value());
    }

    TEST_F(HttpConnectionPoolTest, ReleaseClosesConnectionsBeyondLimit)
    {
        m_configuration.maxIdleServerConnectionsPerHost = 1;
        HttpConnectionPool pool(m_configuration);
        auto pair1 = CreateSocketPair();
        auto pair2 = CreateSocketPair();

        pool.Release("key", std::move(pair1.client));
        pool.Release("key", std::move(pair2.client));

        EXPECT_TRUE(pool.Acquire("key").has_value());
        EXPECT_FALSE(pool.Acquire("key").has_value());

        char byte = 0;
        asio::error_code ec;
        pair2.server.read_some(asio::buffer(&byte, 1), ec);
        EXPECT_EQ(ec, asio::error::eof);
    }

    TEST_F(HttpConnectionPoolTest, AcquireDiscardsExpiredConnection)
    {
        m_configuration.serverIdleTimeout = 0ms;
        HttpConnectionPool pool(m_configuration);
        auto pair = CreateSocketPair();

        pool.Release("key", std::move(pair.client));

        EXPECT_FALSE(pool.Acquire("key").has_value());
    }

    TEST_F(HttpConnectionPoolTest, AcquireDiscardsConnectionClosedByPeer)
    {
        HttpConnectionPool pool(m_configuration);
        auto pair = CreateSocketPair();

        pool.Release("key", std::move(pair.client));
        pair.server.close();
        std::this_thread::sleep_for(50ms);

        EXPECT_FALSE(pool.Acquire("key").has_value());
    }

    TEST_F(HttpConnectionPoolTest, AcquireDiscardsConnectionWithPendingData)
    {
        HttpConnectionPool pool(m_configuration);
        auto pair = CreateSocketPair();

        pool.Release("key", std::move(pair.client));
        asio::write(pair.server, asio::buffer("x", 1));
        std::this_thread::sleep_for(50ms);

        EXPECT_FALSE(pool.Acquire("key").has_value());
    }

    TEST_F(HttpConnectionPoolTest, AcquireSkipsDeadConnectionAndReturnsAliveOne)
    {
        HttpConnectionPool pool(m_configuration);
        auto alive = CreateSocketPair();
        auto dead = CreateSocketPair();
        auto alivePort = alive.client.local_endpoint().port();

        pool.Release("key", std::move(alive.client));
        pool.Release("key", std::move(dead.client));
        dead.server.close();
        std::this_thread::sleep_for(50ms);

        auto socket = pool.Acquire("key");
        ASSERT_TRUE(socket.has_value());
        EXPECT_EQ(socket->local_endpoint().port(), alivePort);
        EXPECT_FALSE(pool.Acquire("key").has_value());
    }

    TEST_F(HttpConnectionPoolTest, ClearClosesAllConnections)
    {
        HttpConnectionPool pool(m_configuration);
        auto pair1 = CreateSocketPair();
        auto pair2 = CreateSocketPair();

        pool.Release("key1", std::move(pair1.client));
        pool.Release("key2", std::move(pair2.client));
        pool.Clear();

        EXPECT_FALSE(pool.Acquire("key1").has_value());
        EXPECT_FALSE(pool.Acquire("key2").has_value());

        char byte = 0;
        asio::error_code ec;
        pair1.server.read_some(asio::buffer(&byte, 1), ec);
        EXPECT_EQ(ec, asio::error::eof);
    }

    TEST_F(HttpConnectionPoolTest, DestructorClosesAllConnections)
    {
        auto pair = CreateSocketPair();
        {
            HttpConnectionPool pool(m_configuration);
            pool.Release("key", std::move(pair.client));
        }

        char byte = 0;
        asio::error_code ec;
        pair.server.read_some(asio::buffer(&byte, 1), ec);
        EXPECT_EQ(ec, asio::error::eof);
    }
}
