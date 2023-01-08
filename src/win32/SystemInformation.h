//
// SystemInformation.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once

namespace win32
{
    class SystemInformation
    {
    public:
        static void PrintAll();
        static void PrintWindowsVersion();
        static void PrintSystemInfo();
        static void PrintProcessInfo();
        static void PrintUsername();

    private:
        static const char* MapProcessIntegrityLevel(DWORD dwIntegrityLevel);
    };
}
