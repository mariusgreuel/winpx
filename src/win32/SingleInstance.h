//
// SingleInstance.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once

namespace win32
{
    class SingleInstance
    {
    public:
        SingleInstance(const char* id);
        ~SingleInstance();

        SingleInstance(const SingleInstance&) = delete;
        SingleInstance& operator=(const SingleInstance&) = delete;

        void Acquire();
        void Release();
        bool IsFirst() const;

    private:
        std::string m_id;
        HANDLE m_hMutex = nullptr;
        DWORD m_dwLastError = ERROR_SUCCESS;
    };
}
