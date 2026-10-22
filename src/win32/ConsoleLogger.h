//
// ConsoleLogger.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include "Logger.h"

namespace win32
{
    class ConsoleLogger
    {
    public:
        static bool IsEnabled();
        static void Start();
        static void Stop();
    };
}
