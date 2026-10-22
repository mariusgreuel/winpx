//
// HttpHelper.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include "HttpRequestMessage.h"
#include "Uri.h"

#include <string>
#include <string_view>

namespace winpx
{
    Uri GetAbsoluteUrl(const HttpRequestMessage& request);
    bool IsLocalhost(std::string_view host);
    std::string PickProxyFromList(const std::string& list, std::string_view url);
    std::string RedactHead(std::string head);
}
