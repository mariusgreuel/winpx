//
// ProxyAuthenticationSession.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include "AuthenticationSchemes.h"
#include "ProxyConfiguration.h"

namespace winpx
{
    class ProxyAuthenticationSession
    {
    public:
        explicit ProxyAuthenticationSession(const ProxyConfiguration& proxyConfiguration, AuthenticationScheme authenticationScheme, const std::string& servicePrincipalName);
        ~ProxyAuthenticationSession() noexcept;

        ProxyAuthenticationSession(const ProxyAuthenticationSession&) = delete;
        ProxyAuthenticationSession& operator=(const ProxyAuthenticationSession&) = delete;

    public:
        SECURITY_STATUS GetNextToken(const std::vector<std::byte>& inputToken, std::vector<std::byte>& outputToken);

    private:
        void AcquireCredentials(AuthenticationScheme authenticationScheme);
        void CloseHandles();

        static const wchar_t* GetSecurityPackage(AuthenticationScheme authenticationScheme);

    private:
        static constexpr ULONG_PTR SecInvalidHandle = ~ULONG_PTR(0);
        const ProxyConfiguration& m_proxyConfiguration;
        std::string m_servicePrincipalName;
        CredHandle m_hCredential{ SecInvalidHandle, SecInvalidHandle };
        CtxtHandle m_hContext{ SecInvalidHandle, SecInvalidHandle };
    };
}
