//
// WppTracing.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include <pch.h>

#include "WppTracing.h"
#include "WppTracing.tmh"

#include "SystemInformation.h"

namespace win32
{
    WppTracing::WppTracing()
    {
        WPP_INIT_TRACING(nullptr);

        DoTraceMessage(WppInfo, "WPP Tracing started");

        SystemInformation::PrintAll();
    }

    WppTracing::~WppTracing()
    {
        DoTraceMessage(WppInfo, "WPP Tracing stopped");

        WPP_CLEANUP();
    }
}
