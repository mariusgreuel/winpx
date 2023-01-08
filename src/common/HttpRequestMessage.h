//
// HttpRequestMessage.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include "HttpMessage.h"

namespace winpx
{
    class HttpRequestMessage : public HttpMessage
    {
    public:
        HttpRequestMessage() = default;
        HttpRequestMessage(std::string method, std::string requestUri);

        HttpRequestMessage(const HttpRequestMessage&) = delete;
        HttpRequestMessage& operator=(const HttpRequestMessage&) = delete;
        HttpRequestMessage(HttpRequestMessage&&) = default;
        HttpRequestMessage& operator=(HttpRequestMessage&&) = default;

        std::string GetMethod() const;
        void SetMethod(std::string method);

        std::string GetRequestUri() const;
        void SetRequestUri(std::string requestUri);

        std::string GetRequestLine() const;
        std::string GetHead() const;
        std::string ToString() const;

        void Parse(std::string_view message);
        void Parse(std::string_view message, size_t headSize);

    private:
        void ParseHead(std::string_view buffer);
        void ParseRequestLine(std::string_view buffer);

    private:
        std::string m_method;
        std::string m_requestUri;
    };
}
