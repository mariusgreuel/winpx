//
// HttpMessage.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include "HttpHeaders.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace winpx
{
    class HttpMessage
    {
    protected:
        HttpMessage() = default;

    public:
        std::string GetBody() const;
        void SetBody(std::string body);

        bool IsHttpVersion(uint8_t majorVersion, uint8_t minorVersion) const;

        HttpHeaders& GetHeaders();
        const HttpHeaders& GetHeaders() const;

    protected:
        void ParseHeaders(std::string_view headers);
        void ParseHeader(std::string_view header);
        void ParseHttpVersion(std::string_view buffer);
        static std::tuple<std::string, std::string> SplitMessage(std::string_view message, size_t headSize);
        static bool GetHeaderLine(std::string_view request, size_t& offset, std::string_view& line);

    protected:
        uint8_t m_majorVersion = 1;
        uint8_t m_minorVersion = 1;
        HttpHeaders m_headers;
        std::string m_body;
    };
}
