//
// HttpMessageFraming.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include <optional>

namespace winpx
{
    struct HttpMessageFraming
    {
        bool chunked = false;
        std::optional<size_t> contentLength;
    };
}
