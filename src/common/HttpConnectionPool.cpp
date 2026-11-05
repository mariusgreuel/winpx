//
// HttpConnectionPool.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include "pch.h"

#include "HttpConnectionPool.h"
#include "HttpConnectionPool.tmh"

namespace winpx
{
    using namespace std::literals::string_view_literals;

    HttpConnectionPool::HttpConnectionPool(const HttpConfiguration& httpConfiguration) : m_httpConfiguration(httpConfiguration)
    {
    }

    HttpConnectionPool::~HttpConnectionPool()
    {
        Clear();
    }

    std::optional<asio::ip::tcp::socket> HttpConnectionPool::Acquire(const std::string& key)
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: key=%!str!", key);

        auto now = std::chrono::steady_clock::now();

        while (true)
        {
            auto connection = PopConnection(key);
            if (!connection)
            {
                DoTraceMessage(WppVerbose, "No hit for '%!str!'", key);
                return std::nullopt;
            }

            if (!IsStale(*connection, now))
            {
                if (IsConnectionAlive(connection->socket))
                {
                    asio::error_code ec;
                    DoTraceMessage(WppVerbose, "Reusing connection [%u/%u] from pool", connection->socket.local_endpoint(ec).port(), connection->socket.remote_endpoint(ec).port());
                    return std::move(connection->socket);
                }
                else
                {
                    asio::error_code ec;
                    DoTraceMessage(WppWarning, "Connection [%u/%u] died", connection->socket.local_endpoint(ec).port(), connection->socket.remote_endpoint(ec).port());
                }
            }

            asio::error_code ec;
            DoTraceMessage(WppVerbose, "Closing stale socket [%u/%u]", connection->socket.local_endpoint(ec).port(), connection->socket.remote_endpoint(ec).port());
            connection->socket.close(ec);
        }
    }

    void HttpConnectionPool::Release(const std::string& key, asio::ip::tcp::socket socket)
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: key=%!str!", key);

        RemoveStaleConnections(key);

        if (!socket.is_open())
        {
            return;
        }

        asio::error_code ec;
        DoTraceMessage(WppVerbose, "Releasing socket [%u/%u] back to the pool", socket.local_endpoint(ec).port(), socket.remote_endpoint(ec).port());

        auto oldConnection = PushConnection(key, { std::move(socket), std::chrono::steady_clock::now() });
        if (oldConnection)
        {
            DoTraceMessage(WppVerbose, "Evicting socket [%u/%u] because the pool is full", oldConnection->socket.local_endpoint(ec).port(), oldConnection->socket.remote_endpoint(ec).port());
            oldConnection->socket.close(ec);
        }
    }

    void HttpConnectionPool::Clear()
    {
        DoTraceMessage(WppVerbose, "%!FUNC!");

        std::lock_guard lock(m_mutex);

        for (auto& [key, connections] : m_pool)
        {
            for (auto& connection : connections)
            {
                asio::error_code ec;
                DoTraceMessage(WppVerbose, "Closing socket [%u/%u]", connection.socket.local_endpoint(ec).port(), connection.socket.remote_endpoint(ec).port());
                connection.socket.close(ec);
            }
        }

        m_pool.clear();
    }

    std::string HttpConnectionPool::MakeKey(bool direct, std::string_view host, uint16_t port)
    {
        return std::format("{}:{}:{}", direct ? "direct"sv : "proxy"sv, host, port);
    }

    bool HttpConnectionPool::IsStale(const HttpConnectionPool::Connection& connection, std::chrono::steady_clock::time_point now) const
    {
        return now - connection.idleSince >= m_httpConfiguration.serverIdleTimeout;
    }

    void HttpConnectionPool::RemoveStaleConnections(const std::string& key)
    {
        auto now = std::chrono::steady_clock::now();

        std::vector<asio::ip::tcp::socket> staleSockets;

        {
            std::lock_guard lock(m_mutex);

            auto it = m_pool.find(key);
            if (it == m_pool.end())
            {
                return;
            }

            auto& connections = it->second;
            std::erase_if(connections, [&](Connection& connection) {
                if (IsStale(connection, now))
                {
                    staleSockets.push_back(std::move(connection.socket));
                    return true;
                }
                else
                {
                    return false;
                }
            });

            if (connections.empty())
            {
                m_pool.erase(it);
            }
        }

        for (auto& socket : staleSockets)
        {
            asio::error_code ec;
            DoTraceMessage(WppVerbose, "Closing stale socket [%u/%u]", socket.local_endpoint(ec).port(), socket.remote_endpoint(ec).port());
            socket.close(ec);
        }
    }

    std::optional<HttpConnectionPool::Connection> HttpConnectionPool::PushConnection(const std::string& key, Connection connection)
    {
        std::lock_guard lock(m_mutex);

        auto& connections = m_pool[key];
        connections.push_back(std::move(connection));
        if (connections.size() > m_httpConfiguration.maxIdleServerConnectionsPerHost)
        {
            Connection oldConnection = std::move(connections.front());
            connections.pop_front();
            return oldConnection;
        }

        return std::nullopt;
    }

    std::optional<HttpConnectionPool::Connection> HttpConnectionPool::PopConnection(const std::string& key)
    {
        std::lock_guard lock(m_mutex);

        auto it = m_pool.find(key);
        if (it == m_pool.end())
        {
            return std::nullopt;
        }

        auto& connections = it->second;
        if (!connections.empty())
        {
            Connection connection = std::move(connections.back());
            connections.pop_back();

            if (connections.empty())
            {
                m_pool.erase(it);
            }

            return connection;
        }

        m_pool.erase(it);
        return std::nullopt;
    }

    bool HttpConnectionPool::IsConnectionAlive(asio::ip::tcp::socket& socket)
    {
        if (!socket.is_open())
        {
            return false;
        }

        asio::error_code ec;
        socket.non_blocking(true, ec);
        if (ec)
        {
            return false;
        }

        char byte = 0;
        socket.receive(asio::buffer(&byte, 1), asio::socket_base::message_peek, ec);
        bool alive = ec == asio::error::would_block;

        socket.non_blocking(false, ec);
        if (ec)
        {
            return false;
        }

        return alive;
    }
}
