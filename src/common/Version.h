//
// Version.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#define VERSION_MAJOR 1
#define VERSION_MINOR 5
#define VERSION_PATCH 74
#define VERSION_QFE 0

#ifndef RC_INVOKED

#include <format>
#include <string>
#include <string_view>

inline std::string GetProgramVersion()
{
    using namespace std::literals::string_view_literals;

    auto result = std::format("WinPX v{}.{}.{}", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);

    std::string_view gitCommitHash = GIT_COMMIT_HASH;
    if (!gitCommitHash.empty() && gitCommitHash != "unknown"sv)
    {
        result += std::format(" ({})", gitCommitHash.substr(0, 8));
    }

#ifdef _DEBUG
    result += " (Debug)";
#endif

    return result;
}

#endif
