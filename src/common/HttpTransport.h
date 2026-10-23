//
// HttpTransport.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include "HttpConfiguration.h"
#include "HttpMessageFraming.h"
#include "HttpReplayBuffer.h"

#include <asio/awaitable.hpp>
#include <asio/ip/tcp.hpp>

#include <cstdint>
#include <string>
#include <string_view>

namespace winpx
{
    class HttpTransport
    {
    public:
        static constexpr size_t untilEof = std::numeric_limits<size_t>::max();

        struct TunnelResult
        {
            size_t clientBytesSent = 0;
            size_t clientBytesReceived = 0;
            size_t serverBytesSent = 0;
            size_t serverBytesReceived = 0;
        };

        HttpTransport(const HttpConfiguration& httpConfiguration);

        asio::awaitable<asio::ip::tcp::socket> ConnectTo(std::string_view host, uint16_t port) const;
        asio::awaitable<void> DrainAndClose(asio::ip::tcp::socket& client) const;

        size_t ParseChunkSize(std::string_view line) const;

        asio::awaitable<std::string> ReadData(asio::ip::tcp::socket& socket, std::string& buffer, size_t length) const;
        asio::awaitable<std::string> ReadChunkedData(asio::ip::tcp::socket& socket, std::string& buffer) const;
        asio::awaitable<std::string> ReadBody(asio::ip::tcp::socket& socket, std::string& buffer, const HttpMessageFraming& framing) const;

        asio::awaitable<size_t> WriteData(asio::ip::tcp::socket& socket, std::string_view buffer) const;

        asio::awaitable<size_t> StreamData(asio::ip::tcp::socket& source, asio::ip::tcp::socket& destination, std::string& buffer, size_t length, HttpReplayBuffer* replayBuffer = nullptr) const;
        asio::awaitable<size_t> StreamChunkedData(asio::ip::tcp::socket& source, asio::ip::tcp::socket& destination, std::string& buffer, HttpReplayBuffer* replayBuffer = nullptr) const;
        asio::awaitable<size_t> StreamRequestBody(asio::ip::tcp::socket& client, asio::ip::tcp::socket& server, std::string& buffer, const HttpMessageFraming& framing, HttpReplayBuffer* replayBuffer = nullptr) const;
        asio::awaitable<size_t> StreamResponseBody(asio::ip::tcp::socket& server, asio::ip::tcp::socket& client, std::string& buffer, const HttpMessageFraming& framing) const;

        asio::awaitable<TunnelResult> Tunnel(asio::ip::tcp::socket& client, asio::ip::tcp::socket& server, std::string_view clientLeftover, std::string_view serverLeftover);
        asio::awaitable<size_t> RelayDataOneWay(asio::ip::tcp::socket& source, asio::ip::tcp::socket& destination) const;

    private:
        void AppendToReplayBuffer(HttpReplayBuffer* replayBuffer, std::string_view data) const;

    private:
        const HttpConfiguration& m_httpConfiguration;
    };
}
