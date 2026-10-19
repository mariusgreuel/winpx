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

    uint8_t HttpMessage::GetMajorVersion() const
    {
        return m_majorVersion;
    }

    uint8_t HttpMessage::GetMinorVersion() const
    {
        return m_minorVersion;
    }

    bool HttpMessage::IsHttpVersion(uint8_t majorVersion, uint8_t minorVersion) const
    {
        return m_majorVersion == majorVersion && m_minorVersion == minorVersion;
    }

    HttpHeaders& HttpMessage::GetHeaders()
    {
        return m_headers;
    }

    const HttpHeaders& HttpMessage::GetHeaders() const
    {
        return m_headers;
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
            throw HttpError(HttpStatusCode::BadRequest, "Invalid header line");
        }

        auto name = header.substr(0, colon);
        if (!std::ranges::all_of(name, IsFieldNameChar))
        {
            throw HttpError(HttpStatusCode::BadRequest, "Invalid header name");
        }

        auto value = Trim(header.substr(colon + 1));
        if (!std::ranges::all_of(value, IsFieldValueChar))
        {
            throw HttpError(HttpStatusCode::BadRequest, "Invalid header value");
        }

        m_headers.Add(name, value);
    }

    void HttpMessage::ParseHttpVersion(std::string_view buffer)
    {
        if (buffer.size() < 8 ||
            ToUpper(buffer[0]) != 'H' || ToUpper(buffer[1]) != 'T' || ToUpper(buffer[2]) != 'T' || ToUpper(buffer[3]) != 'P' || buffer[4] != '/')
        {
            throw HttpError(HttpStatusCode::BadRequest, "Invalid HTTP version");
        }

        auto versions = SplitString(buffer.substr(5), '.');
        if (versions.size() != 2)
        {
            throw HttpError(HttpStatusCode::BadRequest, "Invalid HTTP version");
        }

        try
        {
            m_majorVersion = ParseDec<uint8_t>(versions[0]);
            m_minorVersion = ParseDec<uint8_t>(versions[1]);
        }
        catch (const std::exception&)
        {
            throw HttpError(HttpStatusCode::BadRequest, "Invalid HTTP version");
        }
    }

    std::tuple<std::string, std::string> HttpMessage::SplitMessage(std::string_view message, size_t headSize)
    {
        return { std::string(message.substr(0, headSize)), std::string(message.substr(headSize)) };
    }

    bool HttpMessage::GetHeaderLine(std::string_view request, size_t& offset, std::string_view& line)
    {
        if (offset >= request.size())
        {
            throw HttpError(HttpStatusCode::BadRequest, "Invalid HTTP header");
        }

        auto lf = request.find_first_of('\n', offset);
        auto cr = request.find_first_of('\r', offset);
        if (lf == std::string_view::npos)
        {
            throw HttpError(HttpStatusCode::BadRequest, "Invalid HTTP header");
        }
        else if (lf == offset || cr > lf)
        {
            throw HttpError(HttpStatusCode::BadRequest, "Bare LF in HTTP header detected");
        }
        else if (cr != lf - 1)
        {
            throw HttpError(HttpStatusCode::BadRequest, "Bare CR in HTTP header detected");
        }

        line = request.substr(offset, cr - offset);
        offset = lf + 1;

        return !line.empty();
    }
}
