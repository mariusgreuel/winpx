//
// HttpRequestMessage.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include "HttpMessage.h"

#include "Uri.h"

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

        const Uri& GetRequestUri() const;
        void SetRequestUri(Uri requestUri);

        std::string GetRequestLine() const;
        std::string GetHead() const;
        std::string GetRedactedHead() const;
        std::string ToString() const;

        void Parse(std::string_view message);
        void Parse(std::string_view message, size_t headSize);

    private:
        void ParseHead(std::string_view buffer);
        void ParseRequestLine(std::string_view buffer);

    private:
        std::string m_method;
        Uri m_requestUri;
    };
}
