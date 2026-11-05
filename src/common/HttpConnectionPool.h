//
// HttpConnectionPool.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include "HttpConfiguration.h"

#include <chrono>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace winpx
{
    class HttpConnectionPool
    {
    public:
        explicit HttpConnectionPool(const HttpConfiguration& httpConfiguration);
        ~HttpConnectionPool();

        HttpConnectionPool(const HttpConnectionPool&) = delete;
        HttpConnectionPool& operator=(const HttpConnectionPool&) = delete;

        std::optional<asio::ip::tcp::socket> Acquire(const std::string& key);
        void Release(const std::string& key, asio::ip::tcp::socket socket);
        void Clear();

        static std::string MakeKey(bool direct, std::string_view host, uint16_t port);

    private:
        struct Connection
        {
            asio::ip::tcp::socket socket;
            std::chrono::steady_clock::time_point idleSince;
        };

        bool IsStale(const Connection& connection, std::chrono::steady_clock::time_point now) const;
        void RemoveStaleConnections(const std::string& key);
        std::optional<Connection> PushConnection(const std::string& key, Connection connection);
        std::optional<Connection> PopConnection(const std::string& key);
        static bool IsConnectionAlive(asio::ip::tcp::socket& socket);

    private:
        const HttpConfiguration& m_httpConfiguration;
        std::mutex m_mutex;
        std::unordered_map<std::string, std::deque<Connection>> m_pool;
    };
}
