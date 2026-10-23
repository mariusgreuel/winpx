//
// HttpTransport.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include "pch.h"

#include "HttpTransport.h"
#include "HttpTransport.tmh"

#include "StringHelper.h"

namespace winpx
{
    using namespace std::literals::string_view_literals;
    using namespace asio::experimental::awaitable_operators;

    static constexpr size_t maxBufferSize = 0x10000;

    HttpTransport::HttpTransport(const HttpConfiguration& httpConfiguration) : m_httpConfiguration(httpConfiguration)
    {
    }

    asio::awaitable<asio::ip::tcp::socket> HttpTransport::ConnectTo(std::string_view host, uint16_t port) const
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: host=%!sv!, port=%u", host, port);

        asio::any_io_executor executor = co_await asio::this_coro::executor;
        asio::ip::tcp::socket socket(executor);
        asio::ip::tcp::resolver resolver(executor);
        asio::ip::tcp::resolver::results_type results = co_await resolver.async_resolve(
            host,
            std::to_string(port),
            asio::cancel_after(m_httpConfiguration.resolveTimeout, asio::use_awaitable));
        co_await asio::async_connect(
            socket,
            results,
            asio::cancel_after(m_httpConfiguration.connectTimeout, asio::use_awaitable));
        co_return socket;
    }

    asio::awaitable<void> HttpTransport::DrainAndClose(asio::ip::tcp::socket& client) const
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: [%u/%u] Closing connection...",
            client.local_endpoint().port(),
            client.remote_endpoint().port());

        asio::error_code ec;
        client.shutdown(asio::socket_base::shutdown_send, ec);
        if (!ec)
        {
            std::string discard(0x4000, '\0');

            size_t totalBytes = 0;
            while (totalBytes < m_httpConfiguration.maxDrainSize)
            {
                size_t bytesRead = co_await client.async_read_some(
                    asio::buffer(discard),
                    asio::redirect_error(asio::cancel_after(m_httpConfiguration.drainTimeout, asio::use_awaitable), ec));
                if (ec)
                    break;

                totalBytes += bytesRead;
            }
        }

        client.close(ec);
    }

    size_t HttpTransport::ParseChunkSize(std::string_view line) const
    {
        size_t chunkSize = 0;
        try
        {
            if (!line.ends_with("\r\n"sv))
            {
                throw std::runtime_error("Invalid chunk size");
            }

            line.remove_suffix(2);

            size_t pos = line.find(';');
            if (pos != std::string_view::npos)
            {
                line = TrimEnd(line.substr(0, pos));
            }

            chunkSize = ParseHex<size_t>(line);
        }
        catch (const std::exception&)
        {
            throw std::runtime_error("Invalid chunk size");
        }

        if (chunkSize > m_httpConfiguration.maxChunkSize)
        {
            throw std::runtime_error("Chunk size exceeds maximum allowed value");
        }

        return chunkSize;
    }

    asio::awaitable<std::string> HttpTransport::ReadData(asio::ip::tcp::socket& socket, std::string& buffer, size_t length) const
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: [%u/%u], length=%Iu", socket.local_endpoint().port(), socket.remote_endpoint().port(), length);


        if (length == 0)
        {
            co_return std::string();
        }

        std::string result;

        auto dynamicBuffer = asio::dynamic_buffer(buffer, maxBufferSize);

        size_t bytesRemaining = length;
        while (bytesRemaining > 0)
        {
            asio::error_code ec;

            if (dynamicBuffer.size() == 0)
            {
                size_t bytesToRead = std::min(bytesRemaining, maxBufferSize);
                co_await asio::async_read(
                    socket,
                    dynamicBuffer,
                    asio::transfer_exactly(bytesToRead),
                    asio::redirect_error(asio::cancel_after(m_httpConfiguration.readTimeout, asio::use_awaitable), ec));
                if (ec && (ec != asio::error::eof || length != untilEof))
                {
                    DoTraceMessage(WppWarning, "Failed to read data: %!str!", ec.message());
                    throw asio::system_error(ec);
                }
            }

            size_t bytesToCopy = std::min(dynamicBuffer.size(), bytesRemaining);
            result += buffer.substr(0, bytesToCopy);
            dynamicBuffer.consume(bytesToCopy);
            bytesRemaining -= bytesToCopy;

            if (ec == asio::error::eof)
            {
                break;
            }
        }

        co_return result;
    }

    asio::awaitable<std::string> HttpTransport::ReadChunkedData(asio::ip::tcp::socket& socket, std::string& buffer) const
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: [%u/%u]", socket.local_endpoint().port(), socket.remote_endpoint().port());

        auto dynamicBuffer = asio::dynamic_buffer(buffer, maxBufferSize);

        auto readLine = [&]() -> asio::awaitable<std::string_view> {
            auto lineSize = co_await asio::async_read_until(
                socket,
                dynamicBuffer,
                '\n',
                asio::cancel_after(m_httpConfiguration.readTimeout, asio::use_awaitable));
            if (lineSize < 2 || buffer[lineSize - 2] != '\r')
            {
                throw std::runtime_error("Invalid chunk");
            }

            co_return std::string_view(buffer).substr(0, lineSize);
        };

        std::string result;

        while (true)
        {
            auto line = co_await readLine();
            size_t chunkSize = ParseChunkSize(line);
            dynamicBuffer.consume(line.size());

            if (chunkSize == 0)
            {
                break;
            }

            result += co_await ReadData(socket, buffer, chunkSize);

            auto crlf = co_await readLine();
            if (crlf != "\r\n"sv)
            {
                throw std::runtime_error("Invalid chunk");
            }

            dynamicBuffer.consume(crlf.size());
        }

        bool isLastLine = false;
        while (!isLastLine)
        {
            auto line = co_await readLine();
            isLastLine = line == "\r\n"sv;
            dynamicBuffer.consume(line.size());
        }

        co_return result;
    }

    asio::awaitable<std::string> HttpTransport::ReadBody(asio::ip::tcp::socket& socket, std::string& buffer, const HttpMessageFraming& framing) const
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: [%u/%u]", socket.local_endpoint().port(), socket.remote_endpoint().port());

        try
        {
            if (framing.chunked)
            {
                co_return co_await ReadChunkedData(socket, buffer);
            }
            else if (framing.contentLength)
            {
                co_return co_await ReadData(socket, buffer, *framing.contentLength);
            }
            else
            {
                co_return std::string();
            }
        }
        catch (const std::exception& e)
        {
            DoTraceMessage(WppWarning, "Failed to read body: %s", e.what());
            throw;
        }
    }

    asio::awaitable<size_t> HttpTransport::WriteData(asio::ip::tcp::socket& socket, std::string_view buffer) const
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: [%u/%u], buffer.size=%Iu",
            socket.local_endpoint().port(),
            socket.remote_endpoint().port(),
            buffer.size());

        if (buffer.empty())
            co_return 0;

        try
        {
            co_return co_await asio::async_write(
                socket,
                asio::buffer(buffer),
                asio::cancel_after(m_httpConfiguration.writeTimeout, asio::use_awaitable));
        }
        catch (const std::exception& e)
        {
            DoTraceMessage(WppWarning, "Failed to write data: %s", e.what());
            throw;
        }
    }

    asio::awaitable<size_t> HttpTransport::StreamData(asio::ip::tcp::socket& source, asio::ip::tcp::socket& destination, std::string& buffer, size_t length, HttpReplayBuffer* replayBuffer) const
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: [%u/%u -> %u/%u], length=%Iu",
            source.local_endpoint().port(),
            source.remote_endpoint().port(),
            destination.local_endpoint().port(),
            destination.remote_endpoint().port(),
            length);

        if (length == 0)
            co_return 0;

        auto dynamicBuffer = asio::dynamic_buffer(buffer, maxBufferSize);

        size_t totalBytes = 0;
        size_t bytesRemaining = length;
        while (bytesRemaining > 0)
        {
            asio::error_code ec;

            if (dynamicBuffer.size() == 0)
            {
                size_t bytesToRead = std::min(bytesRemaining, maxBufferSize);
                co_await asio::async_read(
                    source,
                    dynamicBuffer,
                    asio::transfer_exactly(bytesToRead),
                    asio::redirect_error(asio::cancel_after(m_httpConfiguration.readTimeout, asio::use_awaitable), ec));
                if (ec && (ec != asio::error::eof || length != untilEof))
                {
                    DoTraceMessage(WppWarning, "Failed to read data: %!str!", ec.message());
                    throw asio::system_error(ec);
                }
            }

            size_t bytesToWrite = std::min(dynamicBuffer.size(), bytesRemaining);
            size_t bytesWritten = co_await asio::async_write(
                destination,
                dynamicBuffer.data(0, bytesToWrite),
                asio::cancel_after(m_httpConfiguration.writeTimeout, asio::use_awaitable));
            AppendToReplayBuffer(replayBuffer, std::string_view(buffer).substr(0, bytesWritten));

            dynamicBuffer.consume(bytesWritten);
            bytesRemaining -= bytesWritten;
            totalBytes += bytesWritten;

            if (ec == asio::error::eof)
            {
                break;
            }
        }

        co_return totalBytes;
    }

    asio::awaitable<size_t> HttpTransport::StreamChunkedData(asio::ip::tcp::socket& source, asio::ip::tcp::socket& destination, std::string& buffer, HttpReplayBuffer* replayBuffer) const
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: [%u/%u -> %u/%u]",
            source.local_endpoint().port(),
            source.remote_endpoint().port(),
            destination.local_endpoint().port(),
            destination.remote_endpoint().port());

        auto dynamicBuffer = asio::dynamic_buffer(buffer, maxBufferSize);

        auto readLine = [&]() -> asio::awaitable<std::string_view> {
            auto lineSize = co_await asio::async_read_until(
                source,
                dynamicBuffer,
                '\n',
                asio::cancel_after(m_httpConfiguration.readTimeout, asio::use_awaitable));
            if (lineSize < 2 || buffer[lineSize - 2] != '\r')
            {
                throw std::runtime_error("Invalid chunk");
            }

            co_return std::string_view(buffer).substr(0, lineSize);
        };

        size_t totalBytes = 0;
        while (true)
        {
            auto line = co_await readLine();
            size_t chunkSize = ParseChunkSize(line);
            co_await asio::async_write(
                destination,
                dynamicBuffer.data(0, line.size()),
                asio::cancel_after(m_httpConfiguration.writeTimeout, asio::use_awaitable));
            AppendToReplayBuffer(replayBuffer, line);
            dynamicBuffer.consume(line.size());

            if (chunkSize == 0)
            {
                break;
            }

            totalBytes += co_await StreamData(source, destination, buffer, chunkSize, replayBuffer);

            auto crlf = co_await readLine();
            if (crlf != "\r\n"sv)
            {
                throw std::runtime_error("Invalid chunk");
            }

            co_await asio::async_write(
                destination,
                dynamicBuffer.data(0, crlf.size()),
                asio::cancel_after(m_httpConfiguration.writeTimeout, asio::use_awaitable));
            AppendToReplayBuffer(replayBuffer, crlf);
            dynamicBuffer.consume(crlf.size());
        }

        bool isLastLine = false;
        while (!isLastLine)
        {
            auto line = co_await readLine();
            isLastLine = line == "\r\n"sv;
            co_await asio::async_write(
                destination,
                dynamicBuffer.data(0, line.size()),
                asio::cancel_after(m_httpConfiguration.writeTimeout, asio::use_awaitable));
            AppendToReplayBuffer(replayBuffer, line);
            dynamicBuffer.consume(line.size());
        }

        co_return totalBytes;
    }

    asio::awaitable<size_t> HttpTransport::StreamRequestBody(asio::ip::tcp::socket& client, asio::ip::tcp::socket& server, std::string& buffer, const HttpMessageFraming& framing, HttpReplayBuffer* replayBuffer) const
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: [%u/%u -> %u/%u], captureForReplay=%d",
            client.local_endpoint().port(),
            client.remote_endpoint().port(),
            server.local_endpoint().port(),
            server.remote_endpoint().port(),
            replayBuffer != nullptr);

        try
        {
            size_t totalBytes = 0;

            if (framing.chunked)
            {
                totalBytes = co_await StreamChunkedData(client, server, buffer, replayBuffer);
            }
            else
            {
                size_t contentLength = framing.contentLength.value_or(0);
                totalBytes = co_await StreamData(client, server, buffer, contentLength, replayBuffer);
                if (totalBytes != contentLength)
                {
                    throw std::runtime_error("Incomplete body");
                }
            }

            co_return totalBytes;
        }
        catch (const std::exception& e)
        {
            DoTraceMessage(WppWarning, "Failed to stream body: %s", e.what());
            throw;
        }
    }

    asio::awaitable<size_t> HttpTransport::StreamResponseBody(asio::ip::tcp::socket& server, asio::ip::tcp::socket& client, std::string& buffer, const HttpMessageFraming& framing) const
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: [%u/%u -> %u/%u]",
            server.local_endpoint().port(),
            server.remote_endpoint().port(),
            client.local_endpoint().port(),
            client.remote_endpoint().port());

        try
        {
            size_t totalBytes = 0;

            if (framing.chunked)
            {
                totalBytes = co_await StreamChunkedData(server, client, buffer);
            }
            else if (framing.contentLength)
            {
                totalBytes = co_await StreamData(server, client, buffer, *framing.contentLength);
            }
            else
            {
                totalBytes = co_await StreamData(server, client, buffer, untilEof);
            }

            co_return totalBytes;
        }
        catch (const std::exception& e)
        {
            DoTraceMessage(WppWarning, "Failed to stream body: %s", e.what());
            throw;
        }
    }

    asio::awaitable<HttpTransport::TunnelResult> HttpTransport::Tunnel(asio::ip::tcp::socket& client, asio::ip::tcp::socket& server, std::string_view clientLeftover, std::string_view serverLeftover)
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: [%u/%u <-> %u/%u]",
            client.local_endpoint().port(),
            client.remote_endpoint().port(),
            server.local_endpoint().port(),
            server.remote_endpoint().port());

        TunnelResult result;

        if (!clientLeftover.empty())
        {
            result.serverBytesSent += co_await WriteData(server, clientLeftover);
        }

        if (!serverLeftover.empty())
        {
            result.clientBytesSent += co_await WriteData(client, serverLeftover);
        }

        auto [bytesClientToServer, bytesServerToClient] = co_await (RelayDataOneWay(client, server) && RelayDataOneWay(server, client));
        result.clientBytesSent += bytesServerToClient;
        result.clientBytesReceived += bytesClientToServer;
        result.serverBytesSent += bytesClientToServer;
        result.serverBytesReceived += bytesServerToClient;
        co_return result;
    }

    asio::awaitable<size_t> HttpTransport::RelayDataOneWay(asio::ip::tcp::socket& source, asio::ip::tcp::socket& destination) const
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: [%u/%u -> %u/%u]",
            source.local_endpoint().port(),
            source.remote_endpoint().port(),
            destination.local_endpoint().port(),
            destination.remote_endpoint().port());

        std::vector<char> buffer(maxBufferSize);

        size_t totalBytes = 0;

        try
        {
            while (true)
            {
                size_t bytesRead = co_await source.async_read_some(
                    asio::buffer(buffer),
                    asio::cancel_after(m_httpConfiguration.readTimeout, asio::use_awaitable));
                co_await asio::async_write(
                    destination,
                    asio::buffer(buffer, bytesRead),
                    asio::cancel_after(m_httpConfiguration.writeTimeout, asio::use_awaitable));
                totalBytes += bytesRead;
            }
        }
        catch (const std::system_error& e)
        {
            auto& code = e.code();
            if (code != asio::error::eof &&
                code != std::error_code(WSAECONNABORTED, asio::system_category()) &&
                code != std::error_code(WSAECONNRESET, asio::system_category()))
            {
                DoTraceMessage(WppWarning, "Failed to relay data: %s", e.what());
            }
        }

        asio::error_code ec;
        destination.shutdown(asio::socket_base::shutdown_send, ec);

        co_return totalBytes;
    }

    void HttpTransport::AppendToReplayBuffer(HttpReplayBuffer* replayBuffer, std::string_view data) const
    {
        if (replayBuffer != nullptr)
        {
            replayBuffer->Append(data, m_httpConfiguration.maxReplayBufferSize);
        }
    }
}
