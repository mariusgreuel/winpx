//
// Tools.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include <algorithm>
#include <charconv>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace winpx
{
    inline char ToLower(char ch)
    {
        return ch >= 'A' && ch <= 'Z' ? ch - 'A' + 'a' : ch;
    }

    inline char ToUpper(char ch)
    {
        return ch >= 'a' && ch <= 'z' ? ch - 'a' + 'A' : ch;
    }

    inline std::string ToLower(std::string_view text)
    {
        std::string result(text);
        std::transform(result.begin(), result.end(), result.begin(), [](char ch) { return ToLower(ch); });
        return result;
    }

    inline std::string ToUpper(std::string_view text)
    {
        std::string result(text);
        std::transform(result.begin(), result.end(), result.begin(), [](char ch) { return ToUpper(ch); });
        return result;
    }

    inline bool IEquals(std::string_view text, std::string_view what)
    {
        return std::equal(text.begin(), text.end(), what.begin(), what.end(), [](char ch1, char ch2) -> bool {
            return ToUpper(ch1) == ToUpper(ch2);
        });
    }

    inline bool IStartsWith(std::string_view text, std::string_view what)
    {
        return text.size() >= what.size() && IEquals(text.substr(0, what.size()), what);
    }

    inline bool IContains(std::string_view text, std::string_view what)
    {
        return ToLower(text).find(ToLower(what)) != std::string::npos;
    }

    inline std::string_view Trim(std::string&& text, char ch) = delete;
    inline std::string_view Trim(std::string_view text, char ch)
    {
        size_t start = text.find_first_not_of(ch);
        if (start == std::string_view::npos)
            return {};

        size_t end = text.find_last_not_of(ch);
        return text.substr(start, end - start + 1);
    }

    inline std::string_view Trim(std::string&& text, const char* sequence) = delete;
    inline std::string_view Trim(std::string_view text, const char* sequence = " \t\n\r")
    {
        size_t start = text.find_first_not_of(sequence);
        if (start == std::string_view::npos)
            return {};

        size_t end = text.find_last_not_of(sequence);
        return text.substr(start, end - start + 1);
    }

    inline bool GetSubstring(std::string&& text, size_t& offset, std::string_view& substring, char separator) = delete;
    inline bool GetSubstring(std::string_view text, size_t& offset, std::string_view& substring, char separator)
    {
        auto start = text.find_first_not_of(separator, offset);
        if (start == std::string_view::npos)
        {
            return false;
        }

        auto end = text.find_first_of(separator, start + 1);
        if (end == std::string_view::npos)
        {
            substring = text.substr(start);
            offset = text.size();
        }
        else
        {
            substring = text.substr(start, end - start);
            offset = end + 1;
        }

        return true;
    }

    inline std::vector<std::string_view> SplitString(std::string&& text, char separator) = delete;
    inline std::vector<std::string_view> SplitString(std::string_view text, char separator)
    {
        std::vector<std::string_view> substrings;

        size_t offset = 0;
        std::string_view substring;
        while (GetSubstring(text, offset, substring, separator))
        {
            substrings.push_back(substring);
        }

        return substrings;
    }

    template<typename T = size_t>
    inline T ParseInteger(std::string_view text, int base)
    {
        T value = 0;
        auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value, base);
        if (ec == std::errc::result_out_of_range)
        {
            throw std::out_of_range("Integer is out of range.");
        }
        else if (ec == std::errc::invalid_argument || ptr != text.data() + text.size())
        {
            throw std::invalid_argument("Invalid integer.");
        }
        else
        {
            return value;
        }
    }

    template<typename T = size_t>
    inline T ParseDec(std::string_view text)
    {
        return ParseInteger<T>(text, 10);
    }

    template<typename T = size_t>
    inline T ParseHex(std::string_view text)
    {
        return ParseInteger<T>(text, 16);
    }
}
