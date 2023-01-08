//
// ProxyAuthenticationSession.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include "pch.h"

#include "ProxyAuthenticationSession.h"
#include "ProxyAuthenticationSession.tmh"

#include <win32/Unicode.h>
#include <win32/Win32Error.h>

namespace winpx
{
    using namespace win32;

    ProxyAuthenticationSession::ProxyAuthenticationSession(const ProxyConfiguration& proxyConfiguration, AuthenticationScheme authenticationScheme, const std::string& servicePrincipalName) :
        m_proxyConfiguration(proxyConfiguration),
        m_servicePrincipalName(servicePrincipalName)
    {
        DoTraceMessage(WppObject, "%!FUNC!: authenticationScheme=%!PROXY_AUTHENTICATION_SCHEME!, servicePrincipalName='%!str!'", static_cast<int>(authenticationScheme), servicePrincipalName);

        AcquireCredentials(authenticationScheme);
    }

    ProxyAuthenticationSession::~ProxyAuthenticationSession()
    {
        DoTraceMessage(WppObject, "%!FUNC!");

        CloseHandles();
    }

    SECURITY_STATUS ProxyAuthenticationSession::GetNextToken(const std::vector<std::byte>& inputToken, std::vector<std::byte>& outputToken)
    {
        DoTraceMessage(WppVerbose, "%!FUNC!");

        outputToken.clear();

        SecBuffer input{};
        input.BufferType = SECBUFFER_TOKEN;
        input.cbBuffer = static_cast<unsigned long>(inputToken.size());
        input.pvBuffer = inputToken.empty() ? nullptr : const_cast<std::byte*>(inputToken.data());
        SecBufferDesc inputDesc{ SECBUFFER_VERSION, 1, &input };

        SecBuffer output{};
        output.BufferType = SECBUFFER_TOKEN;
        output.cbBuffer = 0;
        output.pvBuffer = nullptr;
        SecBufferDesc outputDesc{ SECBUFFER_VERSION, 1, &output };

        const ULONG fContextReq = ISC_REQ_CONNECTION | ISC_REQ_ALLOCATE_MEMORY;
        ULONG fContextAttr = 0;

        SECURITY_STATUS status = InitializeSecurityContextW(
            &m_hCredential,
            SecIsValidHandle(&m_hContext) ? &m_hContext : nullptr,
            Unicode::FromUtf8(m_servicePrincipalName).data(),
            fContextReq,
            0,
            SECURITY_NATIVE_DREP,
            inputToken.empty() ? nullptr : &inputDesc,
            0,
            &m_hContext,
            &outputDesc,
            &fContextAttr,
            nullptr);
        if (status == SEC_E_OK || status == SEC_I_CONTINUE_NEEDED)
        {
            if (!m_proxyConfiguration.allowedAuthenticationSchemes.Contains(AuthenticationScheme::Ntlm))
            {
                SecPkgContext_PackageInfoW pkgInfo{};
                SECURITY_STATUS statusQuery = QueryContextAttributesW(&m_hContext, SECPKG_ATTR_PACKAGE_INFO, &pkgInfo);
                if (statusQuery == SEC_E_OK)
                {
                    DoTraceMessage(WppVerbose, "PackageName='%S', Comment='%S'", pkgInfo.PackageInfo->Name, pkgInfo.PackageInfo->Comment);
                    if (wcsstr(pkgInfo.PackageInfo->Name, L"NTLM") != nullptr)
                    {
                        DoTraceMessage(WppWarning, "NTLM fallback detected, failing negotiation.");
                        status = HRESULT_FROM_WIN32(ERROR_NTLM_BLOCKED);
                    }
                }

                FreeContextBuffer(pkgInfo.PackageInfo);
            }

            if (output.pvBuffer != nullptr)
            {
                if (output.cbBuffer > 0)
                {
                    auto buffer = static_cast<const std::byte*>(output.pvBuffer);
                    outputToken.assign(buffer, buffer + output.cbBuffer);
                }

                FreeContextBuffer(output.pvBuffer);
            }

            return status;
        }
        else
        {
            DoTraceMessage(WppError, "InitializeSecurityContext failed: %!WINERROR!", status);

            if (output.pvBuffer != nullptr)
            {
                FreeContextBuffer(output.pvBuffer);
            }

            return status;
        }
    }

    void ProxyAuthenticationSession::AcquireCredentials(AuthenticationScheme authenticationScheme)
    {
        CloseHandles();

        std::wstring securityPackage = GetSecurityPackage(authenticationScheme);

        TimeStamp tsExpiry{};
        SECURITY_STATUS status = AcquireCredentialsHandleW(
            nullptr,
            securityPackage.data(),
            SECPKG_CRED_OUTBOUND,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            &m_hCredential,
            &tsExpiry);
        if (status != SEC_E_OK)
        {
            DoTraceMessage(WppError, "AcquireCredentialsHandle failed: %!WINERROR!", status);
            throw Win32Error(status, "AcquireCredentialsHandle failed.");
        }
    }

    void ProxyAuthenticationSession::CloseHandles()
    {
        if (SecIsValidHandle(&m_hContext))
        {
            DeleteSecurityContext(&m_hContext);
            SecInvalidateHandle(&m_hContext);
        }

        if (SecIsValidHandle(&m_hCredential))
        {
            FreeCredentialsHandle(&m_hCredential);
            SecInvalidateHandle(&m_hCredential);
        }
    }

    const wchar_t* ProxyAuthenticationSession::GetSecurityPackage(AuthenticationScheme authenticationScheme)
    {
        switch (authenticationScheme)
        {
        case AuthenticationScheme::Digest:
            return L"Digest SSP";
        case AuthenticationScheme::Ntlm:
            return L"NTLM";
        case AuthenticationScheme::Negotiate:
            return L"Negotiate";
        default:
            throw std::invalid_argument("Unsupported authentication scheme.");
        }
    }
}
