//
// SingleInstance.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include <pch.h>

#include "SingleInstance.h"

#include "Unicode.h"

namespace win32
{
    SingleInstance::SingleInstance(const char* id) : m_id(id)
    {
        Acquire();
    }

    SingleInstance::~SingleInstance()
    {
        Release();
    }

    void SingleInstance::Acquire()
    {
        Release();

        m_hMutex = CreateMutexW(nullptr, TRUE, Unicode::FromUtf8(m_id).c_str());
        m_dwLastError = GetLastError();
    }

    void SingleInstance::Release()
    {
        if (m_hMutex != nullptr)
        {
            CloseHandle(m_hMutex);
            m_hMutex = nullptr;
        }
    }

    bool SingleInstance::IsFirst() const
    {
        return m_dwLastError != ERROR_ALREADY_EXISTS;
    }
}
