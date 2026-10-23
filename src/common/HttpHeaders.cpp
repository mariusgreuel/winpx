//
// HttpHeaders.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include "pch.h"

#include "HttpHeaders.h"

#include "StringHelper.h"

namespace winpx
{
    void HttpHeaders::Add(std::string_view name, std::string_view value)
    {
        m_headers.emplace_back(HttpHeader{ name, value });
    }

    void HttpHeaders::Add(const HttpHeader& header)
    {
        m_headers.push_back(header);
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

    std::vector<std::string_view> HttpHeaders::GetListValues(std::string_view name) const
    {
        std::vector<std::string_view> values;

        for (const auto& header : m_headers)
        {
            if (IEquals(header.name, name))
            {
                auto value = std::string_view(header.value);

                size_t start = 0;
                while (true)
                {
                    size_t end = value.find(',', start);
                    if (end != std::string_view::npos)
                    {
                        values.push_back(Trim(value.substr(start, end - start)));
                        start = end + 1;
                    }
                    else
                    {
                        values.push_back(Trim(value.substr(start)));
                        break;
                    }
                }
            }
        }

        return values;
    }

    HttpHeaders HttpHeaders::Select(std::function<bool(std::string_view name)> predicate) const
    {
        HttpHeaders result;

        for (const auto& header : m_headers)
        {
            if (predicate(header.name))
            {
                result.Add(header.name, header.value);
            }
        }

        return result;
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
