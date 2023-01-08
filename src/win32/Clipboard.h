//
// Clipboard.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once

namespace win32
{
    class Clipboard
    {
    public:
        ~Clipboard();

        HRESULT Open(HWND hWndNewOwner);
        HRESULT Close();
        HRESULT SetData(UINT uFormat, HANDLE hMemory);
        HRESULT SetData(std::string_view data);

        static HRESULT CopyToClipboard(std::string_view text, HWND hWndNewOwner = nullptr);

    private:
        bool m_bOwned = false;
    };
}
