//
// WinPxServiceInstaller.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include "pch.h"

#include "WinPxServiceInstaller.h"

namespace winpx
{
    WinPxServiceInstaller::WinPxServiceInstaller(const std::string& serviceName)
    {
        this->serviceName = serviceName;
        startName = "NT AUTHORITY\\NetworkService";
        displayName = "WinPX Proxy Server";
        description = "Provides a local proxy server that connects to an enterprise proxy server.";
    }
}
