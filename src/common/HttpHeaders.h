//
// HttpHeaders.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include "HttpHeader.h"

#include <optional>
#include <vector>

namespace winpx
{
    class HttpHeaders
    {
    public:
        HttpHeaders() = default;

        auto begin() const { return m_headers.begin(); }
        auto end() const { return m_headers.end(); }

        void Add(std::string_view name, std::string_view value);
        void Add(const HttpHeaders& headers);
        void Replace(std::string_view name, std::string_view value);
        size_t GetCount() const;
        bool Contains(std::string_view name) const;
        std::optional<std::string_view> GetValue(std::string_view name) const;
        std::vector<std::string_view> GetValues(std::string_view name) const;

        std::string ToString() const;

    private:
        std::vector<HttpHeader> m_headers;
    };
}
