//
// Guid.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include <pch.h>

#include "Guid.h"
#include "Guid.tmh"

#include "Win32Error.h"
#include <format>

namespace win32
{
    Guid::Guid()
    {
        std::memset(static_cast<GUID*>(this), 0, sizeof(GUID));
    }

    Guid& Guid::Create()
    {
        RPC_STATUS status = UuidCreate(this);
        if (status != RPC_S_OK && status != RPC_S_UUID_LOCAL_ONLY)
        {
            DoTraceMessage(WppError, "UuidCreate failed: %!WINERROR!", status);
            throw Win32Error(status, "UuidCreate failed.");
        }

        return *this;
    }

    std::string Guid::ToString() const
    {
        return std::format("{:08X}-{:04X}-{:04X}-{:02X}{:02X}-{:02X}{:02X}{:02X}{:02X}{:02X}{:02X}",
            Data1, Data2, Data3,
            Data4[0], Data4[1], Data4[2], Data4[3], Data4[4], Data4[5], Data4[6], Data4[7]);
    }
}
