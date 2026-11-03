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

        std::lock_guard lock(m_mutex);

        auto it = m_connections.find(key);
        if (it == m_connections.end())
        {
            DoTraceMessage(WppVerbose, "No hit for '%!str!'", key);
            return std::nullopt;
        }

        auto& connections = it->second;
        RemoveStaleConnections(connections, now);

        if (connections.empty())
        {
            DoTraceMessage(WppVerbose, "No suitable socket found");
            m_connections.erase(it);
            return std::nullopt;
        }

        asio::ip::tcp::socket socket = std::move(connections.back().socket);
        connections.pop_back();

        if (connections.empty())
        {
            m_connections.erase(it);
        }

        DoTraceMessage(WppVerbose, "Reusing socket [%u/%u]", socket.local_endpoint().port(), socket.remote_endpoint().port());
        return socket;
    }

    void HttpConnectionPool::Release(const std::string& key, asio::ip::tcp::socket socket)
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: key=%!str!", key);

        if (!socket.is_open())
        {
            return;
        }

        auto now = std::chrono::steady_clock::now();

        std::lock_guard lock(m_mutex);

        auto& connections = m_connections[key];
        RemoveStaleConnections(connections, now);

        if (connections.size() >= m_httpConfiguration.maxIdleServerConnectionsPerHost)
        {
            CloseSocket(socket, "pool is full");
            return;
        }

        DoTraceMessage(WppVerbose, "Releasing socket [%u/%u] back to the pool", socket.local_endpoint().port(), socket.remote_endpoint().port());
        connections.push_back({ std::move(socket), now });
    }

    void HttpConnectionPool::Clear()
    {
        DoTraceMessage(WppVerbose, "%!FUNC!");

        std::lock_guard lock(m_mutex);

        for (auto& [key, connections] : m_connections)
        {
            for (auto& connection : connections)
            {
                CloseSocket(connection.socket, "pool cleared");
            }
        }

        m_connections.clear();
    }

    std::string HttpConnectionPool::MakeKey(bool direct, std::string_view host, uint16_t port)
    {
        return std::format("{}:{}:{}", direct ? "direct"sv : "proxy"sv, host, port);
    }

    void HttpConnectionPool::RemoveStaleConnections(std::vector<Connection>& connections, std::chrono::steady_clock::time_point now)
    {
        std::erase_if(connections, [&](auto& connection) {
            if (now - connection.idleSince >= m_httpConfiguration.serverIdleTimeout || !IsConnectionAlive(connection.socket))
            {
                CloseSocket(connection.socket, "stale");
                return true;
            }

            return false;
        });
    }

    void HttpConnectionPool::CloseSocket(asio::ip::tcp::socket& socket, const char* reason)
    {
        asio::error_code ec;
        DoTraceMessage(WppVerbose, "Closing socket [%u/%u]: %s", socket.local_endpoint(ec).port(), socket.remote_endpoint(ec).port(), reason);
        socket.close(ec);
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
        return alive && !ec;
    }
}
