//
// Registry.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include <pch.h>

#include "Registry.h"

#include "Unicode.h"

namespace win32
{
    std::wstring Registry::m_rootKey;

    void Registry::SetRootKey(std::string_view rootKey)
    {
        m_rootKey = Unicode::FromUtf8(rootKey);
    }

    bool Registry::GetValue(std::string_view valueName, bool defaultValue)
    {
        bool value = defaultValue;
        QueryValue(valueName, value);
        return value;
    }

    HRESULT Registry::SetValue(std::string_view valueName, bool value)
    {
        CRegKey key;
        HRESULT hr = OpenKey(key, true);
        if (FAILED(hr))
            return hr;

        LONG nError = key.SetDWORDValue(Unicode::FromUtf8(valueName).c_str(), value ? 1 : 0);
        if (nError != ERROR_SUCCESS)
            return AtlHresultFromWin32(nError);

        return S_OK;
    }

    int Registry::GetValue(std::string_view valueName, int defaultValue)
    {
        int value = defaultValue;
        QueryValue(valueName, value);
        return value;
    }

    HRESULT Registry::SetValue(std::string_view valueName, int value)
    {
        CRegKey key;
        HRESULT hr = OpenKey(key, true);
        if (FAILED(hr))
            return hr;

        LONG nError = key.SetDWORDValue(Unicode::FromUtf8(valueName).c_str(), value);
        if (nError != ERROR_SUCCESS)
            return AtlHresultFromWin32(nError);

        return S_OK;
    }

    std::string Registry::GetValue(std::string_view valueName, std::string_view defaultValue)
    {
        std::string value(defaultValue);
        QueryValue(valueName, value);
        return value;
    }

    HRESULT Registry::SetValue(std::string_view valueName, std::string_view value)
    {
        CRegKey key;
        HRESULT hr = OpenKey(key, true);
        if (FAILED(hr))
            return hr;

        LONG nError = key.SetStringValue(Unicode::FromUtf8(valueName).c_str(), Unicode::FromUtf8(value).c_str());
        if (nError != ERROR_SUCCESS)
            return AtlHresultFromWin32(nError);

        return S_OK;
    }

    HRESULT Registry::QueryValue(std::string_view valueName, bool& value)
    {
        CRegKey key;
        HRESULT hr = OpenKey(key, false);
        if (FAILED(hr))
            return hr;

        DWORD dwValue = 0;
        LONG nError = key.QueryDWORDValue(Unicode::FromUtf8(valueName).c_str(), dwValue);
        if (nError != ERROR_SUCCESS)
            return AtlHresultFromWin32(nError);

        value = dwValue != 0;

        return S_OK;
    }

    HRESULT Registry::QueryValue(std::string_view valueName, int& value)
    {
        CRegKey key;
        HRESULT hr = OpenKey(key, false);
        if (FAILED(hr))
            return hr;

        DWORD dwValue = 0;
        LONG nError = key.QueryDWORDValue(Unicode::FromUtf8(valueName).c_str(), dwValue);
        if (nError != ERROR_SUCCESS)
            return AtlHresultFromWin32(nError);

        value = dwValue;

        return S_OK;
    }

    HRESULT Registry::QueryValue(std::string_view valueName, std::string& value)
    {
        CRegKey key;
        HRESULT hr = OpenKey(key, false);
        if (FAILED(hr))
            return hr;

        auto valueNameW = Unicode::FromUtf8(valueName);

        ULONG nChars = 0;
        LONG nError = key.QueryStringValue(valueNameW.c_str(), nullptr, &nChars);
        if (nError != ERROR_SUCCESS)
            return AtlHresultFromWin32(nError);

        std::wstring valueW(nChars, L'\0');
        nError = key.QueryStringValue(valueNameW.c_str(), valueW.data(), &nChars);
        if (nError != ERROR_SUCCESS)
            return AtlHresultFromWin32(nError);

        if (!valueW.empty() && valueW.back() == L'\0')
            valueW.pop_back();

        value = Unicode::ToUtf8(valueW);
        return S_OK;
    }

    HRESULT Registry::OpenKey(CRegKey& key, bool bCreate)
    {
        if (bCreate)
        {
            LONG nError = key.Create(HKEY_CURRENT_USER, m_rootKey.c_str(), REG_NONE, REG_OPTION_NON_VOLATILE, KEY_READ | KEY_WRITE);
            if (nError != ERROR_SUCCESS)
            {
                return AtlHresultFromWin32(nError);
            }
        }
        else
        {
            LONG nError = key.Open(HKEY_CURRENT_USER, m_rootKey.c_str(), KEY_READ);
            if (nError != ERROR_SUCCESS)
            {
                return AtlHresultFromWin32(nError);
            }
        }

        return S_OK;
    }
}
