//
// HttpResponseMessage.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include "pch.h"

#include "HttpResponseMessage.h"

#include "HttpError.h"
#include "Tools.h"

namespace winpx
{
    HttpResponseMessage::HttpResponseMessage(HttpStatusCode statusCode, std::optional<std::string> reasonPhrase) :
        m_statusCode(statusCode),
        m_reasonPhrase(std::move(reasonPhrase))
    {
    }

    HttpStatusCode HttpResponseMessage::GetStatusCode() const
    {
        return m_statusCode;
    }

    std::string HttpResponseMessage::GetReasonPhrase() const
    {
        return m_reasonPhrase ? *m_reasonPhrase : ToReasonPhrase(m_statusCode);
    }

    std::string HttpResponseMessage::GetStatusLine() const
    {
        return std::format("HTTP/{}.{} {} {}\r\n",
            m_majorVersion,
            m_minorVersion,
            static_cast<uint32_t>(m_statusCode),
            GetReasonPhrase());
    }

    std::string HttpResponseMessage::GetHead() const
    {
        return GetStatusLine() + m_headers.ToString() + "\r\n";
    }

    std::string HttpResponseMessage::ToString() const
    {
        return GetHead() + GetBody();
    }

    void HttpResponseMessage::Parse(std::string_view message)
    {
        auto headEnd = message.find("\r\n\r\n");
        if (headEnd == std::string::npos)
        {
            throw HttpError(HttpStatusCode::BadRequest);
        }

        Parse(message, headEnd + 4);
    }

    void HttpResponseMessage::Parse(std::string_view message, size_t headSize)
    {
        std::string head;
        std::tie(head, m_body) = SplitMessage(message, headSize);
        ParseHead(head);
    }

    void HttpResponseMessage::ParseHead(std::string_view buffer)
    {
        size_t offset = 0;
        std::string_view line;
        if (GetHeaderLine(buffer, offset, line))
        {
            ParseStatusLine(line);
        }

        ParseHeaders(buffer.substr(offset));
    }

    void HttpResponseMessage::ParseStatusLine(std::string_view buffer)
    {
        size_t offset = 0;

        std::string_view httpVersion;
        if (!GetSubstring(buffer, offset, httpVersion, ' '))
            throw HttpError(HttpStatusCode::InternalServerError, "Missing HTTP version.");

        std::string_view statusCode;
        if (!GetSubstring(buffer, offset, statusCode, ' '))
            throw HttpError(HttpStatusCode::InternalServerError, "Missing HTTP status code.");

        ParseHttpVersion(httpVersion);

        if (statusCode.size() != 3)
            throw HttpError(HttpStatusCode::InternalServerError, "Invalid HTTP status code.");

        m_statusCode = static_cast<HttpStatusCode>(ParseDec(statusCode));
        m_reasonPhrase = buffer.substr(offset);
    }
}
