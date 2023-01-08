//
// HttpRequestMessage.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include "pch.h"

#include "HttpRequestMessage.h"

#include "HttpError.h"
#include "Tools.h"

namespace winpx
{
    HttpRequestMessage::HttpRequestMessage(std::string method, std::string requestUri) :
        m_method(std::move(method)),
        m_requestUri(std::move(requestUri))
    {
    }

    std::string HttpRequestMessage::GetMethod() const
    {
        return m_method;
    }

    void HttpRequestMessage::SetMethod(std::string method)
    {
        m_method = std::move(method);
    }

    std::string HttpRequestMessage::GetRequestUri() const
    {
        return m_requestUri;
    }

    void HttpRequestMessage::SetRequestUri(std::string requestUri)
    {
        m_requestUri = std::move(requestUri);
    }

    std::string HttpRequestMessage::GetRequestLine() const
    {
        return std::format("{} {} HTTP/{}.{}\r\n",
            m_method,
            m_requestUri,
            m_majorVersion,
            m_minorVersion);
    }

    std::string HttpRequestMessage::GetHead() const
    {
        return GetRequestLine() + m_headers.ToString() + "\r\n";
    }

    std::string HttpRequestMessage::ToString() const
    {
        return GetHead() + GetBody();
    }

    void HttpRequestMessage::Parse(std::string_view message)
    {
        auto headEnd = message.find("\r\n\r\n");
        if (headEnd == std::string::npos)
        {
            throw HttpError(HttpStatusCode::BadRequest);
        }

        Parse(message, headEnd + 4);
    }

    void HttpRequestMessage::Parse(std::string_view message, size_t headSize)
    {
        std::string head;
        std::tie(head, m_body) = SplitMessage(message, headSize);
        ParseHead(head);
    }

    void HttpRequestMessage::ParseHead(std::string_view buffer)
    {
        size_t offset = 0;
        std::string_view line;
        if (GetHeaderLine(buffer, offset, line))
        {
            ParseRequestLine(line);
        }

        ParseHeaders(buffer.substr(offset));
    }

    void HttpRequestMessage::ParseRequestLine(std::string_view buffer)
    {
        size_t offset = 0;

        std::string_view method;
        if (!GetSubstring(buffer, offset, method, ' '))
            throw HttpError(HttpStatusCode::BadRequest, "Missing request method.");

        std::string_view requestUri;
        if (!GetSubstring(buffer, offset, requestUri, ' '))
            throw HttpError(HttpStatusCode::BadRequest, "Missing request URI.");

        std::string_view version;
        if (!GetSubstring(buffer, offset, version, ' '))
            throw HttpError(HttpStatusCode::BadRequest, "Missing HTTP version.");

        if (!buffer.substr(offset).empty())
            throw HttpError(HttpStatusCode::BadRequest, "Unexpected data after HTTP version.");

        ParseHttpVersion(version);

        m_method = method;
        m_requestUri = requestUri;
    }
}
