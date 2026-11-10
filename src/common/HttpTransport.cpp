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

    void HttpTransport::SetTcpNoDelay(asio::ip::tcp::socket& socket) const
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: [%u/%u], setTcpNoDelay=%!bool!",
            socket.local_endpoint().port(),
            socket.remote_endpoint().port(),
            m_httpConfiguration.setTcpNoDelay);

        if (m_httpConfiguration.setTcpNoDelay)
        {
            asio::error_code ec;
            socket.set_option(asio::ip::tcp::no_delay(true), ec);
            if (ec)
            {
                DoTraceMessage(WppWarning, "Failed to set TCP_NODELAY: %!str!", ec.message());
            }
        }
    }

    asio::awaitable<asio::ip::tcp::socket> HttpTransport::ConnectTo(std::string_view host, uint16_t port) const
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: host=%!sv!, port=%u", host, port);

        asio::any_io_executor executor = co_await asio::this_coro::executor;
        asio::ip::tcp::resolver resolver(executor);
        asio::ip::tcp::resolver::results_type results = co_await resolver.async_resolve(
            host,
            std::to_string(port),
            asio::cancel_after(m_httpConfiguration.resolveTimeout, asio::use_awaitable));

        std::vector<asio::ip::tcp::endpoint> endpoints;
        for (const auto& result : results)
        {
            endpoints.push_back(result.endpoint());
        }

        if (endpoints.empty())
        {
            throw asio::system_error(asio::error::host_not_found);
        }

        SortEndpointsWithAlternatingAddressFamily(endpoints);

        auto delay = AreAllLocalhost(endpoints) ? std::chrono::milliseconds(0) : m_httpConfiguration.connectionAttemptDelay;

        asio::ip::tcp::socket socket(executor);
        co_await asio::co_spawn(
            executor,
            ConnectTo(socket, endpoints, delay),
            asio::cancel_after(m_httpConfiguration.connectTimeout, asio::use_awaitable));
        SetTcpNoDelay(socket);
        co_return socket;
    }

    asio::awaitable<void> HttpTransport::ConnectTo(asio::ip::tcp::socket& result, std::span<const asio::ip::tcp::endpoint> endpoints, std::chrono::milliseconds delay)
    {
        asio::any_io_executor executor = co_await asio::this_coro::executor;
        asio::steady_timer nextAttempt(executor, delay);

        auto connect = [&]() -> asio::awaitable<void> {
            asio::error_code ec;
            asio::ip::tcp::socket socket(executor);
            co_await socket.async_connect(endpoints.front(), asio::redirect_error(asio::use_awaitable, ec));
            if (ec)
            {
                nextAttempt.expires_after(std::chrono::milliseconds::zero());
                throw asio::system_error(ec);
            }

            result = std::move(socket);
        };

        auto connectNext = [&]() -> asio::awaitable<void> {
            co_await nextAttempt.async_wait(asio::use_awaitable);
            co_await ConnectTo(result, endpoints.subspan(1), delay);
        };

        try
        {
            if (endpoints.size() == 1)
            {
                co_await connect();
            }
            else
            {
                co_await (connect() || connectNext());
            }
        }
        catch (const asio::multiple_exceptions& e)
        {
            std::rethrow_exception(e.first_exception());
        }
    }

    asio::awaitable<void> HttpTransport::DrainAndClose(asio::ip::tcp::socket& socket, std::string& buffer) const
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: [%u/%u] Closing connection...",
            socket.local_endpoint().port(),
            socket.remote_endpoint().port());

        asio::error_code ec;
        socket.shutdown(asio::socket_base::shutdown_send, ec);
        if (!ec)
        {
            buffer.resize(maxBufferSize);

            size_t totalBytes = 0;
            while (totalBytes < m_httpConfiguration.maxDrainSize)
            {
                size_t bytesRead = co_await socket.async_read_some(
                    asio::buffer(buffer),
                    asio::redirect_error(asio::cancel_after(m_httpConfiguration.drainTimeout, asio::use_awaitable), ec));
                if (ec)
                    break;

                totalBytes += bytesRead;
            }
        }

        socket.close(ec);
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
        result.reserve(length != untilEof ? length : maxBufferSize);

        size_t bytesRemaining = length;

        if (!buffer.empty())
        {
            size_t bytesToMove = std::min(buffer.size(), bytesRemaining);
            result.append(buffer.data(), bytesToMove);
            buffer.erase(0, bytesToMove);
            bytesRemaining -= bytesToMove;
        }

        if (bytesRemaining == 0)
        {
            co_return result;
        }

        while (bytesRemaining > 0)
        {
            asio::error_code ec;
            size_t bytesInResult = result.size();
            size_t bytesToRead = std::min(bytesRemaining, maxBufferSize);
            result.resize(bytesInResult + bytesToRead);
            size_t bytesRead = co_await socket.async_read_some(
                asio::buffer(result.data() + bytesInResult, bytesToRead),
                asio::redirect_error(asio::cancel_after(m_httpConfiguration.readTimeout, asio::use_awaitable), ec));
            result.resize(bytesInResult + bytesRead);
            if (ec)
            {
                if (ec == asio::error::eof && length == untilEof)
                {
                    break;
                }

                DoTraceMessage(WppWarning, "Failed to read data: %!str!", ec.message());
                throw asio::system_error(ec);
            }

            bytesRemaining -= bytesRead;
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
        {
            co_return 0;
        }

        size_t totalBytes = 0;
        size_t bytesRemaining = length;

        if (!buffer.empty())
        {
            size_t bytesToWrite = std::min(buffer.size(), bytesRemaining);
            size_t bytesWritten = co_await asio::async_write(
                destination,
                asio::buffer(buffer.data(), bytesToWrite),
                asio::cancel_after(m_httpConfiguration.writeTimeout, asio::use_awaitable));
            AppendToReplayBuffer(replayBuffer, std::string_view(buffer.data(), bytesWritten));
            buffer.erase(0, bytesWritten);
            bytesRemaining -= bytesWritten;
            totalBytes += bytesWritten;
        }

        if (bytesRemaining == 0)
        {
            co_return totalBytes;
        }

        buffer.resize(maxBufferSize);

        while (bytesRemaining > 0)
        {
            asio::error_code ec;
            size_t bytesToRead = std::min(bytesRemaining, buffer.size());
            size_t bytesRead = co_await source.async_read_some(
                asio::buffer(buffer.data(), bytesToRead),
                asio::redirect_error(asio::cancel_after(m_httpConfiguration.readTimeout, asio::use_awaitable), ec));
            if (ec)
            {
                if (ec == asio::error::eof && length == untilEof)
                {
                    break;
                }

                DoTraceMessage(WppWarning, "Failed to read data: %!str!", ec.message());
                throw asio::system_error(ec);
            }

            size_t bytesWritten = co_await asio::async_write(
                destination,
                asio::buffer(buffer.data(), bytesRead),
                asio::cancel_after(m_httpConfiguration.writeTimeout, asio::use_awaitable));
            AppendToReplayBuffer(replayBuffer, std::string_view(buffer.data(), bytesWritten));

            bytesRemaining -= bytesWritten;
            totalBytes += bytesWritten;
        }

        buffer.clear();

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

    asio::awaitable<size_t> HttpTransport::StreamRequestBody(asio::ip::tcp::socket& source, asio::ip::tcp::socket& destination, std::string& buffer, const HttpMessageFraming& framing, HttpReplayBuffer* replayBuffer) const
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: [%u/%u -> %u/%u], captureForReplay=%d",
            source.local_endpoint().port(),
            source.remote_endpoint().port(),
            destination.local_endpoint().port(),
            destination.remote_endpoint().port(),
            replayBuffer != nullptr);

        try
        {
            size_t totalBytes = 0;

            if (framing.chunked)
            {
                totalBytes = co_await StreamChunkedData(source, destination, buffer, replayBuffer);
            }
            else
            {
                size_t contentLength = framing.contentLength.value_or(0);
                totalBytes = co_await StreamData(source, destination, buffer, contentLength, replayBuffer);
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

    asio::awaitable<size_t> HttpTransport::StreamResponseBody(asio::ip::tcp::socket& source, asio::ip::tcp::socket& destination, std::string& buffer, const HttpMessageFraming& framing) const
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: [%u/%u -> %u/%u]",
            source.local_endpoint().port(),
            source.remote_endpoint().port(),
            destination.local_endpoint().port(),
            destination.remote_endpoint().port());

        try
        {
            size_t totalBytes = 0;

            if (framing.chunked)
            {
                totalBytes = co_await StreamChunkedData(source, destination, buffer);
            }
            else if (framing.contentLength)
            {
                totalBytes = co_await StreamData(source, destination, buffer, *framing.contentLength);
            }
            else
            {
                totalBytes = co_await StreamData(source, destination, buffer, untilEof);
            }

            co_return totalBytes;
        }
        catch (const std::exception& e)
        {
            DoTraceMessage(WppWarning, "Failed to stream body: %s", e.what());
            throw;
        }
    }

    asio::awaitable<HttpTransport::TunnelResult> HttpTransport::Tunnel(asio::ip::tcp::socket& client, asio::ip::tcp::socket& server, std::string& clientBuffer, std::string& serverBuffer)
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: [%u/%u <-> %u/%u]",
            client.local_endpoint().port(),
            client.remote_endpoint().port(),
            server.local_endpoint().port(),
            server.remote_endpoint().port());

        auto [bytesClientToServer, bytesServerToClient] = co_await (RelayDataOneWay(client, server, clientBuffer) && RelayDataOneWay(server, client, serverBuffer));

        TunnelResult result;
        result.clientBytesSent += bytesServerToClient;
        result.clientBytesReceived += bytesClientToServer;
        result.serverBytesSent += bytesClientToServer;
        result.serverBytesReceived += bytesServerToClient;
        co_return result;
    }

    asio::awaitable<size_t> HttpTransport::RelayDataOneWay(asio::ip::tcp::socket& source, asio::ip::tcp::socket& destination, std::string& buffer) const
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: [%u/%u -> %u/%u]",
            source.local_endpoint().port(),
            source.remote_endpoint().port(),
            destination.local_endpoint().port(),
            destination.remote_endpoint().port());

        size_t totalBytes = 0;

        if (!buffer.empty())
        {
            totalBytes += co_await WriteData(destination, buffer);
            buffer.clear();
        }

        buffer.resize(maxBufferSize);

        try
        {
            while (true)
            {
                size_t bytesRead = co_await source.async_read_some(
                    asio::buffer(buffer),
                    asio::cancel_after(m_httpConfiguration.tunnelTimeout, asio::use_awaitable));
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

        buffer.clear();

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

    bool HttpTransport::AreAllLocalhost(const std::vector<asio::ip::tcp::endpoint>& endpoints)
    {
        return std::all_of(endpoints.begin(), endpoints.end(), [](const auto& endpoint) {
            return endpoint.address().is_loopback();
        });
    }

    void HttpTransport::SortEndpointsWithAlternatingAddressFamily(std::vector<asio::ip::tcp::endpoint>& endpoints)
    {
        for (auto next = endpoints.begin() + 1; next != endpoints.end(); ++next)
        {
            auto alternate = std::find_if(next, endpoints.end(), [&](const auto& endpoint) {
                return endpoint.address().is_v6() != (next - 1)->address().is_v6();
            });
            if (alternate == endpoints.end())
            {
                break;
            }

            std::rotate(next, alternate, alternate + 1);
        }
    }
}
