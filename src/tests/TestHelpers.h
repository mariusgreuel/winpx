//
// TestHelpers.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include <format>
#include <string>
#include <string_view>
#include <vector>

namespace winpx::tests
{
    inline std::string NormalizeCrLf(std::string_view message)
    {
        if (message.find_first_of('\n') == 0)
        {
            message = message.substr(1);
        }

        std::string result(message);

        size_t pos = 0;
        while ((pos = result.find('\n', pos)) != std::string::npos)
        {
            if (pos == 0 || result[pos - 1] != '\r')
            {
                result.insert(pos, "\r");
                pos += 2;
            }
            else
            {
                pos += 1;
            }
        }

        return result;
    }

    inline std::string MakeChunkedBody(const std::vector<std::string_view>& chunks, const std::vector<std::string_view>& trailers = {})
    {
        std::string result;

        for (const auto& chunk : chunks)
        {
            result += std::format("{:X}\r\n{}\r\n", chunk.size(), chunk);
        }

        result += "0\r\n";

        for (const auto& trailer : trailers)
        {
            result += std::format("{}\r\n", trailer);
        }

        return result + "\r\n";
    }
}
