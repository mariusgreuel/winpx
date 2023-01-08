//
// ProxyConfiguration.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include "AuthenticationSchemes.h"
#include "NetworkMode.h"
#include "ProxyMode.h"

namespace winpx
{
    class ProxyConfiguration
    {
    public:
        void LoadFromRegistry();
        void SaveToRegistry() const;

        std::string GetWinPxProxyUrl() const;
        std::string GetWslProxyUrl() const;

        static std::string GenerateSecret(size_t length = 16);

    public:
        ProxyMode proxyMode = ProxyMode::Default;
        NetworkMode networkMode = NetworkMode::Loopback;
        AuthenticationSchemes allowedAuthenticationSchemes = AuthenticationSchemes::Defaults();

        bool firstRun = false;
        bool autoStartApp = false;
        bool autoStartProxy = false;
        bool setEnvironmentVariables = false;
        bool requireLocalAuthentication = false;
        bool reuseSocketAddresses = true;
        uint16_t port = 3128;

        std::string guid;
        std::string version;
        std::string autoConfigUrl;
        std::string httpProxyUrl;
        std::string httpsProxyUrl;
        std::string gatewayUsername;
        std::string gatewayPassword;
        std::string winpxSecret;
        std::string userAgent;
        std::string customAddress;
        std::string diagnosticUrls;

        int threadPoolSize = -1;

        std::chrono::milliseconds resolveTimeout = std::chrono::milliseconds(5000);
        std::chrono::milliseconds connectTimeout = std::chrono::milliseconds(5000);
        std::chrono::milliseconds readTimeout = std::chrono::milliseconds(30000);
        std::chrono::milliseconds writeTimeout = std::chrono::milliseconds(15000);

    private:
        std::string MakeProxyUrl(std::string_view ipAddress) const;
        static void ConfigureAutoStart(bool enabled);
    };
}
