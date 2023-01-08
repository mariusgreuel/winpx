//
// HttpHeader.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include <string>
#include <string_view>

namespace winpx
{
    class HttpHeader
    {
    public:
        HttpHeader() = default;
        HttpHeader(std::string name, std::string value);
        HttpHeader(std::string_view name, std::string_view value);

        std::string name;
        std::string value;
    };
}
