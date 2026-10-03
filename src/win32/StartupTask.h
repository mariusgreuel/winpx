//
// StartupTask.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once

namespace win32
{
    class StartupTask
    {
    public:
        static bool IsEnabled(const char* taskName);
        static bool IsDisabledByUser(const char* taskName);
        static void Enable(const char* taskName, bool enable);
    };
}
