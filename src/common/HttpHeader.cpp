//
// HttpHeader.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include "pch.h"

#include "HttpHeader.h"

namespace winpx
{
    HttpHeader::HttpHeader(std::string name, std::string value) :
        name(std::move(name)),
        value(std::move(value))
    {
    }

    HttpHeader::HttpHeader(std::string_view name, std::string_view value) :
        name(std::string(name)),
        value(std::string(value))
    {
    }
}
