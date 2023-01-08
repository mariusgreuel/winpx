//
// DialogOptionsSecurity.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include "pch.h"

#include "DialogOptionsSecurity.h"
#include "DialogOptionsSecurity.tmh"

#include "OnlineHelp.h"
#include <win32/GroupPolicy.h>
#include <win32/Unicode.h>

namespace winpx
{
    using namespace std::literals::string_view_literals;
    using namespace win32;

    DialogOptionsSecurity::DialogOptionsSecurity(ProxyConfiguration& proxyConfiguration) : m_proxyConfiguration(proxyConfiguration)
    {
        DoTraceMessage(WppObject, "%!FUNC!");

        EnableHelp();
    }

    DialogOptionsSecurity::~DialogOptionsSecurity()
    {
        DoTraceMessage(WppObject, "%!FUNC!");
    }

    LRESULT DialogOptionsSecurity::OnInitDialog(HWND /* hWndCtl */, LPARAM /* lParam */)
    {
        DoTraceMessage(WppGui, "%!FUNC!");

        m_bntAllowNegotiate.Attach(GetDlgItem(IDC_SECURITY_ALLOW_NEGOTIATE));
        m_bntAllowNtlm.Attach(GetDlgItem(IDC_SECURITY_ALLOW_NTLM));
        m_bntAllowDigest.Attach(GetDlgItem(IDC_SECURITY_ALLOW_DIGEST));
        m_bntAllowBasic.Attach(GetDlgItem(IDC_SECURITY_ALLOW_BASIC));
        m_stGatewayUsername.Attach(GetDlgItem(IDC_SECURITY_USERNAME_LABEL));
        m_ecGatewayUsername.Attach(GetDlgItem(IDC_SECURITY_USERNAME));
        m_stGatewayPassword.Attach(GetDlgItem(IDC_SECURITY_PASSWORD_LABEL));
        m_ecGatewayPassword.Attach(GetDlgItem(IDC_SECURITY_PASSWORD));
        m_bntRequireLocalAuthentication.Attach(GetDlgItem(IDC_SECURITY_REQUIRE_LOCAL_AUTH));
        m_stWinPxSecret.Attach(GetDlgItem(IDC_SECURITY_SECRET_LABEL));
        m_ecWinPxSecret.Attach(GetDlgItem(IDC_SECURITY_SECRET));
        m_bntRefreshSecret.Attach(GetDlgItem(IDC_SECURITY_REFRESH_SECRET));
        m_stGroupPolicy.Attach(GetDlgItem(IDC_GROUP_POLICY));

        LoadSettings();
        UpdateGroupPolicy();
        UpdateControls();

        return 0;
    }

    void DialogOptionsSecurity::OnControlUpdateNeeded(UINT nNotifyCode, int nID, HWND /* hWndCtl */)
    {
        DoTraceMessage(WppGui, "%!FUNC!: nNotifyCode=%u, nID=%d", nNotifyCode, nID);

        SaveSettings();
        UpdateControls();
    }

    void DialogOptionsSecurity::OnRefreshWinPxSecret(UINT nNotifyCode, int nID, HWND /* hWndCtl */)
    {
        DoTraceMessage(WppGui, "%!FUNC!: nNotifyCode=%u, nID=%d", nNotifyCode, nID);

        m_proxyConfiguration.winpxSecret = ProxyConfiguration::GenerateSecret();
        m_ecWinPxSecret.SetWindowText(Unicode::FromUtf8(m_proxyConfiguration.winpxSecret).c_str());
    }

    int DialogOptionsSecurity::OnApply()
    {
        DoTraceMessage(WppGui, "%!FUNC!");

        SaveSettings();
        return PSNRET_NOERROR;
    }

    void DialogOptionsSecurity::OnHelp()
    {
        DoTraceMessage(WppGui, "%!FUNC!");

        OnlineHelp::Open("Security"sv);
    }

    void DialogOptionsSecurity::UpdateGroupPolicy()
    {
        DoTraceMessage(WppGui, "%!FUNC!");

        if (GroupPolicy::GetValue("DisableNegotiateAuthentication"sv, false))
        {
            m_bntAllowNegotiate.EnableWindow(FALSE);
            m_bntAllowNegotiate.SetCheck(BST_UNCHECKED);
            m_stGroupPolicy.ShowWindow(SW_SHOW);
        }

        if (GroupPolicy::GetValue("DisableNtlmAuthentication"sv, false))
        {
            m_bntAllowNtlm.EnableWindow(FALSE);
            m_bntAllowNtlm.SetCheck(BST_UNCHECKED);
            m_stGroupPolicy.ShowWindow(SW_SHOW);
        }

        if (GroupPolicy::GetValue("DisableDigestAuthentication"sv, false))
        {
            m_bntAllowDigest.EnableWindow(FALSE);
            m_bntAllowDigest.SetCheck(BST_UNCHECKED);
            m_stGroupPolicy.ShowWindow(SW_SHOW);
        }

        if (GroupPolicy::GetValue("DisableBasicAuthentication"sv, false))
        {
            m_bntAllowBasic.EnableWindow(FALSE);
            m_bntAllowBasic.SetCheck(BST_UNCHECKED);
            m_stGroupPolicy.ShowWindow(SW_SHOW);
        }

        if (GroupPolicy::GetValue("RequireLocalAuthentication"sv, false))
        {
            m_bntRequireLocalAuthentication.EnableWindow(FALSE);
            m_bntRequireLocalAuthentication.SetCheck(BST_CHECKED);
            m_stGroupPolicy.ShowWindow(SW_SHOW);
        }
    }

    void DialogOptionsSecurity::UpdateControls()
    {
        DoTraceMessage(WppGui, "%!FUNC!");

        bool allowBasic = m_bntAllowBasic.GetCheck() == BST_CHECKED;
        m_stGatewayUsername.EnableWindow(allowBasic);
        m_ecGatewayUsername.EnableWindow(allowBasic);
        m_stGatewayPassword.EnableWindow(allowBasic);
        m_ecGatewayPassword.EnableWindow(allowBasic);

        bool requireLocalAuthentication = m_bntRequireLocalAuthentication.GetCheck() == BST_CHECKED;
        m_stWinPxSecret.EnableWindow(requireLocalAuthentication);
        m_ecWinPxSecret.EnableWindow(requireLocalAuthentication);
        m_bntRefreshSecret.EnableWindow(requireLocalAuthentication);
    }

    void DialogOptionsSecurity::LoadSettings()
    {
        CString strTemp;

        auto allowedAuthenticationSchemes = m_proxyConfiguration.allowedAuthenticationSchemes;
        m_bntAllowNegotiate.SetCheck(allowedAuthenticationSchemes.Contains(AuthenticationScheme::Negotiate) ? BST_CHECKED : BST_UNCHECKED);
        m_bntAllowNtlm.SetCheck(allowedAuthenticationSchemes.Contains(AuthenticationScheme::Ntlm) ? BST_CHECKED : BST_UNCHECKED);
        m_bntAllowDigest.SetCheck(allowedAuthenticationSchemes.Contains(AuthenticationScheme::Digest) ? BST_CHECKED : BST_UNCHECKED);
        m_bntAllowBasic.SetCheck(allowedAuthenticationSchemes.Contains(AuthenticationScheme::Basic) ? BST_CHECKED : BST_UNCHECKED);

        m_ecGatewayUsername.SetWindowText(Unicode::FromUtf8(m_proxyConfiguration.gatewayUsername).c_str());
        m_ecGatewayPassword.SetWindowText(Unicode::FromUtf8(m_proxyConfiguration.gatewayPassword).c_str());

        m_bntRequireLocalAuthentication.SetCheck(m_proxyConfiguration.requireLocalAuthentication ? BST_CHECKED : BST_UNCHECKED);
        m_ecWinPxSecret.SetWindowText(Unicode::FromUtf8(m_proxyConfiguration.winpxSecret).c_str());
    }

    void DialogOptionsSecurity::SaveSettings()
    {
        AuthenticationSchemes allowedAuthenticationSchemes;

        if (m_bntAllowNegotiate.GetCheck() == BST_CHECKED)
        {
            allowedAuthenticationSchemes.Add(AuthenticationScheme::Negotiate);
        }

        if (m_bntAllowNtlm.GetCheck() == BST_CHECKED)
        {
            allowedAuthenticationSchemes.Add(AuthenticationScheme::Ntlm);
        }

        if (m_bntAllowDigest.GetCheck() == BST_CHECKED)
        {
            allowedAuthenticationSchemes.Add(AuthenticationScheme::Digest);
        }

        if (m_bntAllowBasic.GetCheck() == BST_CHECKED)
        {
            allowedAuthenticationSchemes.Add(AuthenticationScheme::Basic);
        }

        m_proxyConfiguration.allowedAuthenticationSchemes = allowedAuthenticationSchemes;

        CString strGatewayUsername;
        m_ecGatewayUsername.GetWindowText(strGatewayUsername);
        m_proxyConfiguration.gatewayUsername = Unicode::ToUtf8(strGatewayUsername.GetString());

        CString strGatewayPassword;
        m_ecGatewayPassword.GetWindowText(strGatewayPassword);
        m_proxyConfiguration.gatewayPassword = Unicode::ToUtf8(strGatewayPassword.GetString());

        m_proxyConfiguration.requireLocalAuthentication = m_bntRequireLocalAuthentication.GetCheck() == BST_CHECKED;

        CString strWinPxSecret;
        m_ecWinPxSecret.GetWindowText(strWinPxSecret);
        m_proxyConfiguration.winpxSecret = Unicode::ToUtf8(strWinPxSecret.GetString());
    }
}
