//
// WinPxServiceInstaller.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include <win32/ServiceInstaller.h>

namespace winpx
{
    class WinPxServiceInstaller : public win32::ServiceInstaller
    {
    public:
        WinPxServiceInstaller(const std::string& serviceName);

        WinPxServiceInstaller(const WinPxServiceInstaller&) = delete;
        WinPxServiceInstaller& operator=(const WinPxServiceInstaller&) = delete;
    };
}
