//
// HttpHeaders.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include "pch.h"

#include "HttpHeaders.h"

#include "Tools.h"

namespace winpx
{
    void HttpHeaders::Add(std::string_view name, std::string_view value)
    {
        m_headers.emplace_back(HttpHeader{ name, value });
    }

    void HttpHeaders::Add(const HttpHeaders& headers)
    {
        for (const auto& header : headers.m_headers)
        {
            m_headers.push_back(header);
        }
    }

    void HttpHeaders::Replace(std::string_view name, std::string_view value)
    {
        bool found = false;

        for (auto& header : m_headers)
        {
            if (IEquals(header.name, name))
            {
                header.value = value;
                found = true;
            }
        }

        if (!found)
        {
            Add(name, value);
        }
    }

    size_t HttpHeaders::GetCount() const
    {
        return m_headers.size();
    }

    bool HttpHeaders::Contains(std::string_view name) const
    {
        for (const auto& header : m_headers)
        {
            if (IEquals(header.name, name))
            {
                return true;
            }
        }

        return false;
    }

    std::optional<std::string_view> HttpHeaders::GetValue(std::string_view name) const
    {
        for (const auto& header : m_headers | std::views::reverse)
        {
            if (IEquals(header.name, name))
            {
                return header.value;
            }
        }

        return std::nullopt;
    }

    std::vector<std::string_view> HttpHeaders::GetValues(std::string_view name) const
    {
        std::vector<std::string_view> values;

        for (const auto& header : m_headers)
        {
            if (IEquals(header.name, name))
            {
                values.push_back(header.value);
            }
        }

        return values;
    }

    std::string HttpHeaders::ToString() const
    {
        std::string result;

        for (const auto& header : m_headers)
        {
            result += std::format("{}: {}\r\n", header.name, header.value);
        }

        return result;
    }
}
