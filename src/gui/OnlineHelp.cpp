//
// OnlineHelp.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include "pch.h"

#include "OnlineHelp.h"

#include <win32/GroupPolicy.h>
#include <win32/Registry.h>
#include <win32/UrlLauncher.h>

namespace winpx
{
    using namespace std::literals::string_view_literals;
    using namespace win32;

    void OnlineHelp::Open(std::string_view helpId)
    {
        auto helpKey = std::format("HelpUrl{}", helpId);
        auto url = GroupPolicy::GetValue(helpKey, Registry::GetValue(helpKey, "https://github.com/mariusgreuel/winpx"sv));
        UrlLauncher::Launch(url);
    }
}
