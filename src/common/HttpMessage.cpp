//
// HttpMessage.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include "pch.h"

#include "HttpMessage.h"

#include "HttpError.h"
#include "StringHelper.h"

namespace winpx
{
    using namespace std::literals::string_view_literals;

    static bool IsFieldNameChar(char ch)
    {
        switch (ch)
        {
        case '!':
        case '#':
        case '$':
        case '%':
        case '&':
        case '\'':
        case '*':
        case '+':
        case '-':
        case '.':
        case '^':
        case '_':
        case '`':
        case '|':
        case '~':
            return true;
        default:
            return (ch >= '0' && ch <= '9') ||
                   (ch >= 'A' && ch <= 'Z') ||
                   (ch >= 'a' && ch <= 'z');
        }
    }

    static bool IsFieldValueChar(char ch)
    {
        auto uch = static_cast<unsigned char>(ch);
        return uch == '\t' || (uch >= 0x20 && uch != 0x7F);
    }

    std::string HttpMessage::GetBufferLeftover() const
    {
        return m_body;
    }

    std::string HttpMessage::GetBody() const
    {
        return m_body;
    }

    void HttpMessage::SetBody(std::string body)
    {
        m_body = std::move(body);
    }

    uint8_t HttpMessage::GetVersion() const
    {
        return m_version;
    }

    HttpHeaders& HttpMessage::GetHeaders()
    {
        return m_headers;
    }

    const HttpHeaders& HttpMessage::GetHeaders() const
    {
        return m_headers;
    }

    HttpMessageFraming HttpMessage::GetFraming() const
    {
        auto contentLengths = m_headers.GetListValues("Content-Length"sv);
        auto transferEncodings = m_headers.GetListValues("Transfer-Encoding"sv);

        if (!transferEncodings.empty())
        {
            if (!contentLengths.empty())
            {
                throw HttpError(HttpStatusCode::BadRequest, "Both Content-Length and Transfer-Encoding headers are present");
            }

            if (m_version == 10)
            {
                throw HttpError(HttpStatusCode::BadRequest, "Transfer-Encoding is not allowed in HTTP/1.0 requests");
            }

            if (transferEncodings.size() != 1 || !IEquals(transferEncodings[0], "chunked"sv))
            {
                throw HttpError(HttpStatusCode::BadRequest, "Unsupported transfer encoding");
            }

            return { true, std::nullopt };
        }
        else if (!contentLengths.empty())
        {
            std::optional<size_t> contentLength;

            for (auto value : contentLengths)
            {
                size_t length = 0;
                try
                {
                    length = ParseDec<size_t>(value);
                }
                catch (const std::exception&)
                {
                    throw HttpError(HttpStatusCode::BadRequest, "Invalid Content-Length header.");
                }

                if (contentLength && *contentLength != length)
                {
                    throw HttpError(HttpStatusCode::BadRequest, "Conflicting Content-Length headers.");
                }

                contentLength = length;
            }

            return { false, contentLength };
        }
        else
        {
            return { false, std::nullopt };
        }
    }

    void HttpMessage::ParseHeaders(std::string_view headers)
    {
        size_t offset = 0;
        std::string_view line;
        while (GetHeaderLine(headers, offset, line))
        {
            ParseHeader(line);
        }
    }

    void HttpMessage::ParseHeader(std::string_view header)
    {
        auto colon = header.find(':');
        if (colon == 0 || colon == std::string_view::npos)
        {
            throw std::runtime_error("Invalid header line");
        }

        auto name = header.substr(0, colon);
        if (!std::ranges::all_of(name, IsFieldNameChar))
        {
            throw std::runtime_error("Invalid header name");
        }

        auto value = Trim(header.substr(colon + 1));
        if (!std::ranges::all_of(value, IsFieldValueChar))
        {
            throw std::runtime_error("Invalid header value");
        }

        m_headers.Add(name, value);
    }

    void HttpMessage::ParseHttpVersion(std::string_view buffer)
    {
        if (buffer.size() != 8 ||
            buffer[0] != 'H' ||
            buffer[1] != 'T' ||
            buffer[2] != 'T' ||
            buffer[3] != 'P' ||
            buffer[4] != '/' ||
            buffer[5] != '1' ||
            buffer[6] != '.' ||
            (buffer[7] != '0' && buffer[7] != '1'))
        {
            throw std::runtime_error("Invalid HTTP version");
        }

        m_version = buffer[7] == '0' ? 10 : 11;
    }

    std::tuple<std::string, std::string> HttpMessage::SplitMessage(std::string_view message, size_t headSize)
    {
        return { std::string(message.substr(0, headSize)), std::string(message.substr(headSize)) };
    }

    bool HttpMessage::GetHeaderLine(std::string_view request, size_t& offset, std::string_view& line)
    {
        if (offset >= request.size())
        {
            throw std::runtime_error("Invalid HTTP header");
        }

        auto lf = request.find_first_of('\n', offset);
        auto cr = request.find_first_of('\r', offset);
        if (lf == std::string_view::npos)
        {
            throw std::runtime_error("Invalid HTTP header");
        }
        else if (lf == offset || cr > lf)
        {
            throw std::runtime_error("Bare LF in HTTP header detected");
        }
        else if (cr != lf - 1)
        {
            throw std::runtime_error("Bare CR in HTTP header detected");
        }

        line = request.substr(offset, cr - offset);
        offset = lf + 1;

        return !line.empty();
    }
}
