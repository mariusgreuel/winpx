//
// WinMain.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include "pch.h"

#include "WinMain.tmh"

#include "MainWindow.h"
#include <win32/GroupPolicy.h>
#include <win32/Registry.h>
#include <win32/WppTracing.h>

#include <crtdbg.h>

using namespace std::literals::string_view_literals;
using namespace win32;
using namespace winpx;

namespace winpx
{
    class Module : public CAtlExeModuleT<Module>
    {
    public:
        int Run()
        {
            Registry::SetRootKey("Software\\Marius Greuel\\WinPX"sv);
            GroupPolicy::SetRootKey("Software\\Policies\\Marius Greuel\\WinPX"sv);

            if (!AtlInitCommonControls(ICC_STANDARD_CLASSES))
            {
                DoTraceMessage(WppError, "AtlInitCommonControls failed");
                return 1;
            }

            MainWindow mainWindow;
            HWND hWnd = mainWindow.Create();
            if (hWnd == nullptr)
            {
                DWORD dwError = GetLastError();
                if (dwError != ERROR_CANCELLED && dwError != ERROR_SINGLE_INSTANCE_APP)
                {
                    DoTraceMessage(WppError, "MainWindow::Create failed: %!WINERROR!", dwError);
                }

                return 1;
            }

            RunMessageLoop();
            return 0;
        }
    };
}

int WINAPI _tWinMain(HINSTANCE, HINSTANCE, LPTSTR, int)
{
    WppTracing wpp;

    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_DELAY_FREE_MEM_DF);

    try
    {
        Module module;
        return module.Run();
    }
    catch (std::exception& e)
    {
        DoTraceMessage(WppError, "std::exception: %s", e.what());
        return 1;
    }
}
