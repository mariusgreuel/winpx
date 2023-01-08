//
// GroupPolicy.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include <pch.h>

#include "GroupPolicy.h"

#include "Unicode.h"

namespace win32
{
    std::wstring GroupPolicy::m_rootKey;

    void GroupPolicy::SetRootKey(std::string_view rootKey)
    {
        m_rootKey = Unicode::FromUtf8(rootKey);
    }

    bool GroupPolicy::GetValue(std::string_view valueName, bool defaultValue)
    {
        auto valueNameW = Unicode::FromUtf8(valueName);
        bool value = defaultValue;

        HRESULT hr = QueryValue(HKEY_LOCAL_MACHINE, m_rootKey.c_str(), valueNameW.c_str(), value);
        if (SUCCEEDED(hr))
        {
            return value;
        }

        hr = QueryValue(HKEY_CURRENT_USER, m_rootKey.c_str(), valueNameW.c_str(), value);
        if (SUCCEEDED(hr))
        {
            return value;
        }

        return defaultValue;
    }

    int GroupPolicy::GetValue(std::string_view valueName, int defaultValue)
    {
        auto valueNameW = Unicode::FromUtf8(valueName);
        int value = defaultValue;

        HRESULT hr = QueryValue(HKEY_LOCAL_MACHINE, m_rootKey.c_str(), valueNameW.c_str(), value);
        if (SUCCEEDED(hr))
        {
            return value;
        }

        hr = QueryValue(HKEY_CURRENT_USER, m_rootKey.c_str(), valueNameW.c_str(), value);
        if (SUCCEEDED(hr))
        {
            return value;
        }

        return defaultValue;
    }

    std::string GroupPolicy::GetValue(std::string_view valueName, std::string_view defaultValue)
    {
        auto valueNameW = Unicode::FromUtf8(valueName);
        std::string value(defaultValue);

        HRESULT hr = QueryValue(HKEY_LOCAL_MACHINE, m_rootKey.c_str(), valueNameW.c_str(), value);
        if (SUCCEEDED(hr))
        {
            return value;
        }

        hr = QueryValue(HKEY_CURRENT_USER, m_rootKey.c_str(), valueNameW.c_str(), value);
        if (SUCCEEDED(hr))
        {
            return value;
        }

        return std::string(defaultValue);
    }

    HRESULT GroupPolicy::QueryValue(HKEY hKeyParent, LPCTSTR pszKeyName, LPCTSTR pszValueName, bool& value)
    {
        CRegKey key;
        HRESULT hr = OpenKey(hKeyParent, pszKeyName, false, key);
        if (FAILED(hr))
            return hr;

        DWORD dwValue = 0;
        LONG nError = key.QueryDWORDValue(pszValueName, dwValue);
        if (nError != ERROR_SUCCESS)
            return HRESULT_FROM_WIN32(nError);

        value = dwValue != 0;

        return S_OK;
    }

    HRESULT GroupPolicy::QueryValue(HKEY hKeyParent, LPCTSTR pszKeyName, LPCTSTR pszValueName, int& value)
    {
        CRegKey key;
        HRESULT hr = OpenKey(hKeyParent, pszKeyName, false, key);
        if (FAILED(hr))
            return hr;

        DWORD dwValue = 0;
        LONG nError = key.QueryDWORDValue(pszValueName, dwValue);
        if (nError != ERROR_SUCCESS)
            return HRESULT_FROM_WIN32(nError);

        value = static_cast<int>(dwValue);

        return S_OK;
    }

    HRESULT GroupPolicy::QueryValue(HKEY hKeyParent, LPCTSTR pszKeyName, LPCTSTR pszValueName, std::string& value)
    {
        CRegKey key;
        HRESULT hr = OpenKey(hKeyParent, pszKeyName, false, key);
        if (FAILED(hr))
            return hr;

        ULONG nChars = 0;
        LONG nError = key.QueryStringValue(pszValueName, nullptr, &nChars);
        if (nError != ERROR_SUCCESS)
            return AtlHresultFromWin32(nError);

        std::wstring valueW(nChars, L'\0');
        nError = key.QueryStringValue(pszValueName, valueW.data(), &nChars);
        if (nError != ERROR_SUCCESS)
            return AtlHresultFromWin32(nError);

        if (!valueW.empty() && valueW.back() == L'\0')
            valueW.pop_back();

        value = Unicode::ToUtf8(valueW);
        return S_OK;
    }

    HRESULT GroupPolicy::OpenKey(HKEY hKeyParent, LPCTSTR pszKeyName, bool bCreate, CRegKey& key)
    {
        if (bCreate)
        {
            LONG nError = key.Create(hKeyParent, pszKeyName, REG_NONE, REG_OPTION_NON_VOLATILE, KEY_READ | KEY_WRITE);
            if (nError != ERROR_SUCCESS)
            {
                return HRESULT_FROM_WIN32(nError);
            }
        }
        else
        {
            LONG nError = key.Open(hKeyParent, pszKeyName, KEY_READ);
            if (nError != ERROR_SUCCESS)
            {
                return HRESULT_FROM_WIN32(nError);
            }
        }

        return S_OK;
    }
}
