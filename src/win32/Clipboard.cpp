//
// Clipboard.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include <pch.h>

#include "Clipboard.h"
#include "Clipboard.tmh"

#include "Unicode.h"

namespace win32
{
    Clipboard::~Clipboard()
    {
        Close();
    }

    HRESULT Clipboard::Open(HWND hWndNewOwner)
    {
        if (m_bOwned)
            return S_FALSE;

        if (!::OpenClipboard(hWndNewOwner))
        {
            DWORD dwError = GetLastError();
            DoTraceMessage(WppError, "OpenClipboard failed: %!WINERROR!", dwError);
            return HRESULT_FROM_WIN32(dwError);
        }

        m_bOwned = true;
        return S_OK;
    }

    HRESULT Clipboard::Close()
    {
        if (!m_bOwned)
            return S_FALSE;

        if (!::CloseClipboard())
        {
            DWORD dwError = GetLastError();
            DoTraceMessage(WppError, "CloseClipboard failed: %!WINERROR!", dwError);
            return HRESULT_FROM_WIN32(dwError);
        }

        m_bOwned = false;
        return S_OK;
    }

    HRESULT Clipboard::SetData(UINT uFormat, HANDLE hMemory)
    {
        if (!::EmptyClipboard())
        {
            DWORD dwError = GetLastError();
            DoTraceMessage(WppError, "EmptyClipboard failed: %!WINERROR!", dwError);
            return HRESULT_FROM_WIN32(dwError);
        }

        HANDLE hHandle = ::SetClipboardData(uFormat, hMemory);
        if (hHandle == nullptr)
        {
            DWORD dwError = GetLastError();
            DoTraceMessage(WppError, "SetClipboardData failed: %!WINERROR!", dwError);
            return HRESULT_FROM_WIN32(dwError);
        }

        return S_OK;
    }

    HRESULT Clipboard::SetData(std::string_view text)
    {
        if (text.empty())
        {
            return E_INVALIDARG;
        }

        auto dataW = Unicode::FromUtf8(text);

        size_t nChars = dataW.size();
        size_t nBytes = (nChars + 1) * sizeof(WCHAR);

        HGLOBAL hGlobal = ::GlobalAlloc(GMEM_MOVEABLE, nBytes);
        if (hGlobal == nullptr)
        {
            DWORD dwError = GetLastError();
            DoTraceMessage(WppError, "GlobalAlloc failed: %!WINERROR!", dwError);
            return HRESULT_FROM_WIN32(dwError);
        }

        LPVOID pData = ::GlobalLock(hGlobal);
        if (pData == nullptr)
        {
            ::GlobalFree(hGlobal);
            DWORD dwError = GetLastError();
            DoTraceMessage(WppError, "GlobalLock failed: %!WINERROR!", dwError);
            return HRESULT_FROM_WIN32(dwError);
        }

        std::memcpy(pData, dataW.c_str(), nBytes);

        ::GlobalUnlock(hGlobal);

        HRESULT hr = SetData(CF_UNICODETEXT, hGlobal);
        if (FAILED(hr))
        {
            ::GlobalFree(hGlobal);
            return hr;
        }

        return S_OK;
    }

    HRESULT Clipboard::CopyToClipboard(std::string_view text, HWND hWndNewOwner)
    {
        Clipboard clipboard;
        HRESULT hr = clipboard.Open(hWndNewOwner);
        if (FAILED(hr))
        {
            return hr;
        }

        hr = clipboard.SetData(text);
        if (FAILED(hr))
        {
            return hr;
        }

        return S_OK;
    }
}
