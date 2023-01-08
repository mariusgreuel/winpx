//
// main.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include "pch.h"

#include "main.tmh"

#include "WinPxApplication.h"
#include "WinPxService.h"
#include <win32/ConsoleColor.h>
#include <win32/WppTracing.h>

#include <crtdbg.h>

using namespace win32;

int wmain(int argc, wchar_t** argv)
{
    WppTracing wpp;

    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_DELAY_FREE_MEM_DF);

    try
    {
        SetConsoleCP(CP_UTF8);
        SetConsoleOutputCP(CP_UTF8);
        std::ios_base::sync_with_stdio(false);

        {
            winpx::WinPxService service;
            HRESULT hr = service.Dispatch();
            if (FAILED(hr))
            {
                return 1;
            }
            else if (hr == S_OK)
            {
                return 0;
            }
        }

        winpx::WinPxApplication application;
        return application.Run(argc, argv);
    }
    catch (const std::exception& e)
    {
        DoTraceMessage(WppError, "std::exception: %s", e.what());
        ConsoleColor color(ConsoleColor::ForegroundColor::Red);
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 1;
    }
}
