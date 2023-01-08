//
// ProxyConfiguration.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include "pch.h"

#include "ProxyConfiguration.h"
#include "ProxyConfiguration.tmh"

#include "Version.h"
#include "Wsl.h"
#include <win32/Cryptography.h>
#include <win32/Environment.h>
#include <win32/GroupPolicy.h>
#include <win32/Guid.h>
#include <win32/Registry.h>
#include <win32/Unicode.h>
#include <win32/Win32Error.h>

namespace winpx
{
    using namespace std::literals::string_view_literals;
    using namespace win32;

    static bool GetPolicyOrRegistryValue(std::string_view valueName, bool defaultValue)
    {
        return GroupPolicy::GetValue(valueName, Registry::GetValue(valueName, defaultValue));
    }

    static int GetPolicyOrRegistryValue(std::string_view valueName, int defaultValue)
    {
        return GroupPolicy::GetValue(valueName, Registry::GetValue(valueName, defaultValue));
    }

    static std::string GetPolicyOrRegistryValue(std::string_view valueName, std::string_view defaultValue)
    {
        return GroupPolicy::GetValue(valueName, Registry::GetValue(valueName, defaultValue));
    }

    template<typename T, typename = std::enable_if_t<std::is_integral<T>::value || std::is_enum<T>::value>>
    static T GetPolicyOrRegistryValue(std::string_view valueName, T defaultValue)
    {
        return static_cast<T>(GetPolicyOrRegistryValue(valueName, static_cast<int>(defaultValue)));
    }

    static std::string UnprotectData(const std::string& value)
    {
        try
        {
            if (value.starts_with("protected:"))
            {
                return Cryptography::UnprotectData(value.substr(10));
            }
            else if (value.starts_with("unprotected:"))
            {
                return value.substr(12);
            }
            else
            {
                return {};
            }
        }
        catch (const Win32Error&)
        {
            return {};
        }
    }

    static std::string ProtectData(const std::string& value)
    {
        try
        {
            if (value.empty())
            {
                return {};
            }

            return "protected:" + Cryptography::ProtectData(value);
        }
        catch (const Win32Error&)
        {
            return {};
        }
    }

    void ProxyConfiguration::LoadFromRegistry()
    {
        DoTraceMessage(WppVerbose, "%!FUNC!");

        proxyMode = GetPolicyOrRegistryValue("ProxyMode", ProxyMode::System);
        networkMode = GetPolicyOrRegistryValue("NetworkMode", NetworkMode::Loopback);
        allowedAuthenticationSchemes = AuthenticationSchemes(GetPolicyOrRegistryValue("AllowedAuthenticationSchemes", AuthenticationSchemes::Defaults().ToValue()));

        firstRun = GetPolicyOrRegistryValue("FirstRun", true);
        autoStartApp = GetPolicyOrRegistryValue("AutoStartApp", true);
        autoStartProxy = GetPolicyOrRegistryValue("AutoStartProxy", true);
        setEnvironmentVariables = GetPolicyOrRegistryValue("SetEnvironmentVariables", true);
        requireLocalAuthentication = GetPolicyOrRegistryValue("RequireLocalAuthentication", Environment::IsRemoteDesktopSession());
        reuseSocketAddresses = GetPolicyOrRegistryValue("ReuseSocketAddresses", true);
        port = GetPolicyOrRegistryValue("Port", static_cast<uint16_t>(3128));

        guid = Registry::GetValue("Guid"sv, ""sv);
        version = Registry::GetValue("Version"sv, ""sv);
        autoConfigUrl = GetPolicyOrRegistryValue("AutoConfigUrl"sv, ""sv);
        httpProxyUrl = GetPolicyOrRegistryValue("HttpProxyUrl"sv, ""sv);
        httpsProxyUrl = GetPolicyOrRegistryValue("HttpsProxyUrl"sv, ""sv);
        gatewayUsername = GetPolicyOrRegistryValue("GatewayUsername"sv, ""sv);
        gatewayPassword = UnprotectData(GetPolicyOrRegistryValue("GatewayPassword"sv, ""sv));
        winpxSecret = GetPolicyOrRegistryValue("WinPxSecret"sv, ""sv);

        userAgent = GetPolicyOrRegistryValue("UserAgent"sv, "WinPX/1.0"sv);
        customAddress = GetPolicyOrRegistryValue("CustomAddress"sv, ""sv);
        diagnosticUrls = GetPolicyOrRegistryValue("DiagnosticUrls"sv, "https://www.example.com/"sv);

        threadPoolSize = GetPolicyOrRegistryValue("ThreadPoolSize"sv, -1);

        resolveTimeout = std::chrono::milliseconds(GetPolicyOrRegistryValue("ResolveTimeout"sv, 5000));
        connectTimeout = std::chrono::milliseconds(GetPolicyOrRegistryValue("ConnectTimeout"sv, 5000));
        readTimeout = std::chrono::milliseconds(GetPolicyOrRegistryValue("ReadTimeout"sv, 30000));
        writeTimeout = std::chrono::milliseconds(GetPolicyOrRegistryValue("WriteTimeout"sv, 15000));

        if (guid.length() != 36)
        {
            guid = Guid().Create().ToString();
        }

        if (winpxSecret.empty())
        {
            winpxSecret = GenerateSecret();
        }

        if (customAddress.empty() && networkMode == NetworkMode::LoopbackAndWsl && Wsl::UsesNatNetworkingMode())
        {
            customAddress = Wsl::GetVirtualSwitchAddress();
        }
    }

    void ProxyConfiguration::SaveToRegistry() const
    {
        DoTraceMessage(WppVerbose, "%!FUNC!");

        Registry::SetValue("ProxyMode"sv, proxyMode);
        Registry::SetValue("NetworkMode"sv, networkMode);
        Registry::SetValue("AllowedAuthenticationSchemes"sv, allowedAuthenticationSchemes.ToValue());

        Registry::SetValue("FirstRun"sv, false);
        Registry::SetValue("AutoStartApp"sv, autoStartApp);
        Registry::SetValue("AutoStartProxy"sv, autoStartProxy);
        Registry::SetValue("SetEnvironmentVariables"sv, setEnvironmentVariables);
        Registry::SetValue("RequireLocalAuthentication"sv, requireLocalAuthentication);
        Registry::SetValue("Port"sv, port);

        Registry::SetValue("Guid"sv, guid);
        Registry::SetValue("Version"sv, std::format("{}.{}.{}", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH));
        Registry::SetValue("AutoConfigUrl"sv, autoConfigUrl);
        Registry::SetValue("HttpProxyUrl"sv, httpProxyUrl);
        Registry::SetValue("HttpsProxyUrl"sv, httpsProxyUrl);
        Registry::SetValue("GatewayUsername"sv, gatewayUsername);
        Registry::SetValue("GatewayPassword"sv, ProtectData(gatewayPassword));
        Registry::SetValue("WinPxSecret"sv, winpxSecret);

        ConfigureAutoStart(autoStartApp);
    }

    std::string ProxyConfiguration::MakeProxyUrl(std::string_view ipAddress) const
    {
        if (requireLocalAuthentication)
        {
            return std::format("http://{}@{}:{}", winpxSecret, ipAddress, port);
        }
        else
        {
            return std::format("http://{}:{}", ipAddress, port);
        }
    }

    std::string ProxyConfiguration::GetWinPxProxyUrl() const
    {
        return MakeProxyUrl("127.0.0.1"sv);
    }

    std::string ProxyConfiguration::GetWslProxyUrl() const
    {
        if (Wsl::UsesNatNetworkingMode())
        {
            auto virtualSwitchAddress = Wsl::GetVirtualSwitchAddress();
            if (!virtualSwitchAddress.empty())
            {
                return MakeProxyUrl(virtualSwitchAddress);
            }
        }

        return GetWinPxProxyUrl();
    }

    std::string ProxyConfiguration::GenerateSecret(size_t length)
    {
        static const char charset[] = "ABCDEFGHJKMNPQRSTUVWXYZabcdefghjkmnpqrstuvwxyz23456789";

        std::vector<UCHAR> bytes(length);
        NTSTATUS status = BCryptGenRandom(
            nullptr,
            bytes.data(),
            static_cast<ULONG>(bytes.size()),
            BCRYPT_USE_SYSTEM_PREFERRED_RNG);
        if (!BCRYPT_SUCCESS(status))
        {
            throw Win32Error(status, "BCryptGenRandom failed");
        }

        std::string secret(length, '\0');
        for (size_t i = 0; i < length; ++i)
        {
            secret[i] = charset[bytes[i] % (_countof(charset) - 1)];
        }

        return secret;
    }

    void ProxyConfiguration::ConfigureAutoStart(bool enabled)
    {
        DoTraceMessage(WppVerbose, "%!FUNC!");

        Environment::SetAutoStartApp(
            "WinPX"sv,
            std::format("\"{}\"", Unicode::ToUtf8(Environment::GetProcessPath().native())),
            enabled);
    }
}
