//
// GroupPolicy.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include <string>
#include <string_view>

namespace win32
{
    class GroupPolicy
    {
    public:
        static void SetRootKey(std::string_view rootKey);

        static bool TryGetValue(std::string_view valueName, bool& value);
        static bool TryGetValue(std::string_view valueName, int& value);
        static bool TryGetValue(std::string_view valueName, std::string& value);

        template<typename T, typename = std::enable_if_t<std::is_integral<T>::value || std::is_enum<T>::value>>
        static bool TryGetValue(std::string_view valueName, T& value)
        {
            int intValue = 0;
            if (TryGetValue(valueName, intValue))
            {
                value = static_cast<T>(intValue);
                return true;
            }

            return false;
        }

        static bool GetValue(std::string_view valueName, bool defaultValue);
        static int GetValue(std::string_view valueName, int defaultValue);
        static std::string GetValue(std::string_view valueName, std::string_view defaultValue);

    private:
        static HRESULT QueryValue(HKEY hKeyParent, LPCTSTR pszKeyName, LPCTSTR pszValueName, bool& value);
        static HRESULT QueryValue(HKEY hKeyParent, LPCTSTR pszKeyName, LPCTSTR pszValueName, int& value);
        static HRESULT QueryValue(HKEY hKeyParent, LPCTSTR pszKeyName, LPCTSTR pszValueName, std::string& value);
        static HRESULT OpenKey(HKEY hKeyParent, LPCTSTR pszKeyName, bool bCreate, CRegKey& key);

    private:
        static std::wstring m_rootKey;
    };
}
