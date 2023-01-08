//
// ResourceFile.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include <pch.h>

#include "ResourceFile.h"

namespace win32
{
    ResourceFile::~ResourceFile()
    {
        Unload();
    }

    size_t ResourceFile::GetSize() const
    {
        return m_nSize;
    }

    const BYTE* ResourceFile::GetData() const
    {
        return m_pResource;
    }

    HRESULT ResourceFile::Load(int nResourceName, LPCTSTR pszResourceType)
    {
        return Load(nullptr, MAKEINTRESOURCE(nResourceName), pszResourceType);
    }

    HRESULT ResourceFile::Load(LPCTSTR pszResourceName, LPCTSTR pszResourceType)
    {
        return Load(nullptr, pszResourceName, pszResourceType);
    }

    HRESULT ResourceFile::Load(HINSTANCE hInstance, int nResourceName, LPCTSTR pszResourceType)
    {
        return Load(hInstance, MAKEINTRESOURCE(nResourceName), pszResourceType);
    }

    HRESULT ResourceFile::Load(HINSTANCE hInstance, LPCTSTR pszResourceName, LPCTSTR pszResourceType)
    {
        Unload();

        HRSRC hResInfo = ::FindResource(hInstance, pszResourceName, pszResourceType);
        if (hResInfo == nullptr)
            return HRESULT_FROM_WIN32(GetLastError());

        HGLOBAL hResource = ::LoadResource(hInstance, hResInfo);
        if (hResource == nullptr)
            return HRESULT_FROM_WIN32(GetLastError());

        m_nSize = ::SizeofResource(hInstance, hResInfo);
        if (m_nSize == 0)
            return HRESULT_FROM_WIN32(GetLastError());

        m_pResource = static_cast<const BYTE*>(::LockResource(hResource));
        if (m_pResource == nullptr)
            return HRESULT_FROM_WIN32(GetLastError());

        return S_OK;
    }

    HRESULT ResourceFile::Unload()
    {
        if (m_hResource != nullptr)
        {
            ::FreeResource(m_hResource);

            m_hResource = nullptr;
        }

        m_nSize = 0;
        m_pResource = nullptr;

        return S_OK;
    }
}
