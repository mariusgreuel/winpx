//
// HttpTools.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include "HttpRequestMessage.h"
#include "Uri.h"

#include <string>
#include <string_view>

namespace winpx
{
    class HttpTools
    {
    public:
        static Uri GetAbsoluteUrl(const HttpRequestMessage& request);
        static bool IsLocalhost(std::string_view host);
        static std::string PickProxyFromList(const std::string& list, std::string_view url);
        static std::string NormalizeCrLf(std::string_view message);
    };
}
