//
// Options.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once

namespace winpx
{
    struct Options
    {
        bool configureFirewall = false;
        bool shutdownOldInstance = false;
        bool showOptionsDialog = false;
    };
}
