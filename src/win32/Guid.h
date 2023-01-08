//
// Guid.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include <string>

namespace win32
{
    class Guid : public GUID
    {
    public:
        Guid();
        Guid& Create();
        std::string ToString() const;
    };
}
