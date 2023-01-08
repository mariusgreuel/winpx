//
// ResourceFile.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once

namespace win32
{
    class ResourceFile
    {
    public:
        ResourceFile() = default;
        ~ResourceFile();

        ResourceFile(const ResourceFile&) = delete;
        ResourceFile& operator=(const ResourceFile&) = delete;

        size_t GetSize() const;
        const BYTE* GetData() const;

    public:
        HRESULT Load(int nResourceName, LPCTSTR pszResourceType);
        HRESULT Load(LPCTSTR pszResourceName, LPCTSTR pszResourceType);
        HRESULT Load(HINSTANCE hInstance, int nResourceName, LPCTSTR pszResourceType);
        HRESULT Load(HINSTANCE hInstance, LPCTSTR pszResourceName, LPCTSTR pszResourceType);
        HRESULT Unload();

    private:
        HGLOBAL m_hResource = nullptr;
        size_t m_nSize = 0;
        const BYTE* m_pResource = nullptr;
    };
}
