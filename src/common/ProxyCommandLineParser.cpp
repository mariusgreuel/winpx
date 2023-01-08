//
// ProxyCommandLineParser.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include "pch.h"

#include "ProxyCommandLineParser.h"

#include "Tools.h"

namespace winpx
{
    using namespace std::literals::string_view_literals;

    ProxyCommandLineParser::ProxyCommandLineParser(ProxyConfiguration& proxyConfiguration)
    {
        AddOption("port"sv, [&](std::string_view value) { proxyConfiguration.port = ParseDec<uint16_t>(value); });
        AddOption("mode"sv, [&](std::string_view value) {
            if (value == "system"sv)
                proxyConfiguration.proxyMode = ProxyMode::System;
            else if (value == "direct"sv)
                proxyConfiguration.proxyMode = ProxyMode::Direct;
            else if (value == "manual"sv)
                proxyConfiguration.proxyMode = ProxyMode::Manual;
            else if (value == "pac"sv)
                proxyConfiguration.proxyMode = ProxyMode::AutoConfig;
            else if (value == "wpad"sv)
                proxyConfiguration.proxyMode = ProxyMode::AutoDetect;
            else
                throw std::runtime_error(std::format("Invalid --mode option value '{}'.", value));
        });
        AddOption("pac-url"sv, [&](std::string_view value) {
            if (proxyConfiguration.proxyMode == ProxyMode::Default)
                proxyConfiguration.proxyMode = ProxyMode::AutoConfig;
            proxyConfiguration.autoConfigUrl = value;
        });
        AddOption("http-proxy"sv, [&](std::string_view value) {
            if (proxyConfiguration.proxyMode == ProxyMode::Default)
                proxyConfiguration.proxyMode = ProxyMode::Manual;
            proxyConfiguration.httpProxyUrl = value;
        });
        AddOption("https-proxy"sv, [&](std::string_view value) {
            if (proxyConfiguration.proxyMode == ProxyMode::Default)
                proxyConfiguration.proxyMode = ProxyMode::Manual;
            proxyConfiguration.httpsProxyUrl = value;
        });
        AddOption("auth"sv, [&](std::string_view value) {
            if (value == "any"sv)
                m_allowedAuthenticationSchemes.Add(AuthenticationScheme::Any);
            else if (value == "negotiate"sv)
                m_allowedAuthenticationSchemes.Add(AuthenticationScheme::Negotiate);
            else if (value == "ntlm"sv)
                m_allowedAuthenticationSchemes.Add(AuthenticationScheme::Ntlm);
            else if (value == "digest"sv)
                m_allowedAuthenticationSchemes.Add(AuthenticationScheme::Digest);
            else if (value == "basic"sv)
                m_allowedAuthenticationSchemes.Add(AuthenticationScheme::Basic);
            else if (value == "none"sv)
                m_allowedAuthenticationSchemes.Clear();
            else
                throw std::runtime_error(std::format("Invalid --auth option value '{}'.", value));

            proxyConfiguration.allowedAuthenticationSchemes = m_allowedAuthenticationSchemes;
        });
        AddOption("username"sv, proxyConfiguration.gatewayUsername);
        AddOption("password"sv, proxyConfiguration.gatewayPassword);
        AddOption("secret"sv, [&](std::string_view value) {
            proxyConfiguration.requireLocalAuthentication = true;
            proxyConfiguration.winpxSecret = value;
        });
        AddOption("environment"sv, proxyConfiguration.setEnvironmentVariables);
        AddAlias("p"sv, "port"sv);
        AddAlias("m"sv, "mode"sv);
        AddAlias("e"sv, "environment"sv);
    }
}
