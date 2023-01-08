//
// MitigationPolicy.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once

namespace win32
{
    class MitigationPolicy
    {
    public:
        static void ApplyToProcess();
    };
}
