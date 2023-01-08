//
// Resources.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include <pch.h>

#include "Resources.h"
#include "Resources.tmh"

#include "resource.h"
#include <win32/ResourceFile.h>

namespace winpx
{
    using namespace std::literals::string_view_literals;
    using namespace win32;

    std::string Resources::GetLicenses()
    {
        ResourceFile file;
        HRESULT hr = file.Load(GetModuleHandleW(nullptr), IDR_LICENSES, _T("TEXT"));
        if (FAILED(hr))
        {
            DoTraceMessage(WppError, "Failed to load license resourece: %!HRESULT!", hr);
            return {};
        }
        else
        {
            return std::string(reinterpret_cast<const char*>(file.GetData()), file.GetSize());
        }
    }
}
