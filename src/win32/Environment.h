//
// Environment.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include <filesystem>
#include <string>

namespace win32
{
    class Environment
    {
    public:
        static std::string GetUsername();

        static std::filesystem::path GetProcessPath();

        static bool IsDarkMode();
        static bool IsRemoteDesktopSession();
        static bool IsLocalAdministrator();
        static bool IsProcessAppContainer();
        static DWORD GetProcessIntegrityLevel();

        static HRESULT SetAutoStartApp(std::string_view keyName, std::string_view value, bool enabled);

        static HRESULT SetUserVariable(std::string_view name, std::string_view value);
        static HRESULT RemoveUserVariable(std::string_view name);

        static HRESULT SendSettingChangeNotification(std::string_view area = "Environment");

    private:
        static HRESULT SetOrRemoveUserVariable(std::string_view name, std::string_view value);
    };
}
