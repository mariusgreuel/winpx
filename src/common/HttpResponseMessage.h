//
// HttpResponseMessage.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include "HttpMessage.h"
#include "HttpStatusCode.h"
#include "HttpTools.h"

namespace winpx
{
    class HttpResponseMessage : public HttpMessage, private HttpTools
    {
    public:
        HttpResponseMessage() = default;
        HttpResponseMessage(HttpStatusCode statusCode, std::optional<std::string> reasonPhrase = std::nullopt);

        HttpResponseMessage(const HttpResponseMessage&) = delete;
        HttpResponseMessage& operator=(const HttpResponseMessage&) = delete;
        HttpResponseMessage(HttpResponseMessage&&) = default;
        HttpResponseMessage& operator=(HttpResponseMessage&&) = default;

        HttpStatusCode GetStatusCode() const;
        std::string GetReasonPhrase() const;
        std::string GetStatusLine() const;
        std::string GetHead() const;
        std::string ToString() const;

        void Parse(std::string_view message);
        void Parse(std::string_view message, size_t headSize);

    private:
        void ParseHead(std::string_view buffer);
        void ParseStatusLine(std::string_view buffer);

    private:
        HttpStatusCode m_statusCode = HttpStatusCode::Unknown;
        std::optional<std::string> m_reasonPhrase;
    };
}
