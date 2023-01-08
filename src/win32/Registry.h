//
// Registry.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include <string>
#include <type_traits>

namespace win32
{
    class Registry
    {
    public:
        static void SetRootKey(std::string_view rootKey);

        static bool GetValue(std::string_view valueName, bool defaultValue);
        static HRESULT SetValue(std::string_view valueName, bool value);

        static int GetValue(std::string_view valueName, int defaultValue);
        static HRESULT SetValue(std::string_view valueName, int value);

        static std::string GetValue(std::string_view valueName, std::string_view defaultValue);
        static HRESULT SetValue(std::string_view valueName, std::string_view value);

        template<typename T, typename = std::enable_if_t<std::is_integral<T>::value || std::is_enum<T>::value>>
        static T GetValue(std::string_view valueName, T defaultValue)
        {
            return static_cast<T>(GetValue(valueName, static_cast<int>(defaultValue)));
        }

        template<typename T, typename = std::enable_if_t<std::is_integral<T>::value || std::is_enum<T>::value>>
        static HRESULT SetValue(std::string_view valueName, T value)
        {
            return SetValue(valueName, static_cast<int>(value));
        }

    private:
        static HRESULT OpenKey(CRegKey& key, bool bCreate);
        static HRESULT QueryValue(std::string_view valueName, bool& value);
        static HRESULT QueryValue(std::string_view valueName, int& value);
        static HRESULT QueryValue(std::string_view valueName, std::string& value);

    private:
        static std::wstring m_rootKey;
    };
}
