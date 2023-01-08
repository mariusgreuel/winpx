//
// DialogOptionsGateway.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include "pch.h"

#include "DialogOptionsGateway.h"
#include "DialogOptionsGateway.tmh"

#include "OnlineHelp.h"
#include <win32/GroupPolicy.h>
#include <win32/Unicode.h>

namespace winpx
{
    using namespace std::literals::string_view_literals;
    using namespace win32;

    DialogOptionsGateway::DialogOptionsGateway(ProxyConfiguration& proxyConfiguration) : m_proxyConfiguration(proxyConfiguration)
    {
        DoTraceMessage(WppObject, "%!FUNC!");

        EnableHelp();
    }

    DialogOptionsGateway::~DialogOptionsGateway()
    {
        DoTraceMessage(WppObject, "%!FUNC!");
    }

    LRESULT DialogOptionsGateway::OnInitDialog(HWND /* hWndCtl */, LPARAM /* lParam */)
    {
        DoTraceMessage(WppGui, "%!FUNC!");

        m_bntUseSystemProxy.Attach(GetDlgItem(IDC_GATEWAY_USE_SYSTEM_PROXY));
        m_bntUseWpad.Attach(GetDlgItem(IDC_GATEWAY_USE_AUTO_DETECT));
        m_bntUsePac.Attach(GetDlgItem(IDC_GATEWAY_USE_AUTO_CONFIG));
        m_bntUseManualProxy.Attach(GetDlgItem(IDC_GATEWAY_USE_MANUAL_PROXY));
        m_bntNoProxy.Attach(GetDlgItem(IDC_GATEWAY_NO_PROXY));
        m_stAutoConfigUrl.Attach(GetDlgItem(IDC_GATEWAY_AUTO_CONFIG_LABEL));
        m_ecAutoConfigUrl.Attach(GetDlgItem(IDC_GATEWAY_AUTO_CONFIG_URL));
        m_stHttpProxyUrl.Attach(GetDlgItem(IDC_GATEWAY_HTTP_PROXY_LABEL));
        m_ecHttpProxyUrl.Attach(GetDlgItem(IDC_GATEWAY_HTTP_PROXY_URL));
        m_stHttpsProxyUrl.Attach(GetDlgItem(IDC_GATEWAY_HTTPS_PROXY_LABEL));
        m_ecHttpsProxyUrl.Attach(GetDlgItem(IDC_GATEWAY_HTTPS_PROXY_URL));
        m_stGroupPolicy.Attach(GetDlgItem(IDC_GROUP_POLICY));

        LoadSettings();
        UpdateGroupPolicy();
        UpdateControls();

        return 0;
    }

    void DialogOptionsGateway::OnProxyTypeClicked(UINT nNotifyCode, int nID, HWND /* hWndCtl */)
    {
        DoTraceMessage(WppGui, "%!FUNC!: nNotifyCode=%u, nID=%d", nNotifyCode, nID);

        UpdateControls();
    }

    int DialogOptionsGateway::OnApply()
    {
        DoTraceMessage(WppGui, "%!FUNC!");

        SaveSettings();
        return PSNRET_NOERROR;
    }

    void DialogOptionsGateway::OnHelp()
    {
        DoTraceMessage(WppGui, "%!FUNC!");

        OnlineHelp::Open("Gateway"sv);
    }

    void DialogOptionsGateway::UpdateGroupPolicy()
    {
        DoTraceMessage(WppGui, "%!FUNC!");

        if (GroupPolicy::GetValue("DisableSystemProxy", false))
        {
            m_bntUseSystemProxy.EnableWindow(FALSE);
            m_bntUseSystemProxy.SetCheck(BST_UNCHECKED);
            m_stGroupPolicy.ShowWindow(SW_SHOW);
        }

        if (GroupPolicy::GetValue("DisableWpad", false))
        {
            m_bntUseWpad.EnableWindow(FALSE);
            m_bntUseWpad.SetCheck(BST_UNCHECKED);
            m_stGroupPolicy.ShowWindow(SW_SHOW);
        }

        if (GroupPolicy::GetValue("DisablePac", false))
        {
            m_bntUsePac.EnableWindow(FALSE);
            m_bntUsePac.SetCheck(BST_UNCHECKED);
            m_stGroupPolicy.ShowWindow(SW_SHOW);
        }

        if (GroupPolicy::GetValue("DisableManualProxy", false))
        {
            m_bntUseManualProxy.EnableWindow(FALSE);
            m_bntUseManualProxy.SetCheck(BST_UNCHECKED);
            m_stGroupPolicy.ShowWindow(SW_SHOW);
        }

        if (GroupPolicy::GetValue("DisableNoProxy", false))
        {
            m_bntNoProxy.EnableWindow(FALSE);
            m_bntNoProxy.SetCheck(BST_UNCHECKED);
            m_stGroupPolicy.ShowWindow(SW_SHOW);
        }
    }

    void DialogOptionsGateway::UpdateControls()
    {
        DoTraceMessage(WppGui, "%!FUNC!");

        ProxyMode proxyMode = ProxyMode::System;
        if (m_bntUseSystemProxy.GetCheck() == BST_CHECKED)
        {
            proxyMode = ProxyMode::System;
        }
        else if (m_bntUseWpad.GetCheck() == BST_CHECKED)
        {
            proxyMode = ProxyMode::AutoDetect;
        }
        else if (m_bntUsePac.GetCheck() == BST_CHECKED)
        {
            proxyMode = ProxyMode::AutoConfig;
        }
        else if (m_bntUseManualProxy.GetCheck() == BST_CHECKED)
        {
            proxyMode = ProxyMode::Manual;
        }
        else if (m_bntNoProxy.GetCheck() == BST_CHECKED)
        {
            proxyMode = ProxyMode::Direct;
        }

        m_stAutoConfigUrl.EnableWindow(proxyMode == ProxyMode::AutoConfig);
        m_ecAutoConfigUrl.EnableWindow(proxyMode == ProxyMode::AutoConfig);
        m_stHttpProxyUrl.EnableWindow(proxyMode == ProxyMode::Manual);
        m_ecHttpProxyUrl.EnableWindow(proxyMode == ProxyMode::Manual);
        m_stHttpsProxyUrl.EnableWindow(proxyMode == ProxyMode::Manual);
        m_ecHttpsProxyUrl.EnableWindow(proxyMode == ProxyMode::Manual);
    }

    void DialogOptionsGateway::LoadSettings()
    {
        CString strTemp;

        m_bntUseSystemProxy.SetCheck(m_proxyConfiguration.proxyMode == ProxyMode::System ? BST_CHECKED : BST_UNCHECKED);
        m_bntUseWpad.SetCheck(m_proxyConfiguration.proxyMode == ProxyMode::AutoDetect ? BST_CHECKED : BST_UNCHECKED);
        m_bntUsePac.SetCheck(m_proxyConfiguration.proxyMode == ProxyMode::AutoConfig ? BST_CHECKED : BST_UNCHECKED);
        m_bntUseManualProxy.SetCheck(m_proxyConfiguration.proxyMode == ProxyMode::Manual ? BST_CHECKED : BST_UNCHECKED);
        m_bntNoProxy.SetCheck(m_proxyConfiguration.proxyMode == ProxyMode::Direct ? BST_CHECKED : BST_UNCHECKED);

        m_ecAutoConfigUrl.SetWindowText(Unicode::FromUtf8(m_proxyConfiguration.autoConfigUrl).c_str());
        m_ecHttpProxyUrl.SetWindowText(Unicode::FromUtf8(m_proxyConfiguration.httpProxyUrl).c_str());
        m_ecHttpsProxyUrl.SetWindowText(Unicode::FromUtf8(m_proxyConfiguration.httpsProxyUrl).c_str());
    }

    void DialogOptionsGateway::SaveSettings()
    {
        if (m_bntUseSystemProxy.GetCheck() == BST_CHECKED)
        {
            m_proxyConfiguration.proxyMode = ProxyMode::System;
        }
        else if (m_bntUseWpad.GetCheck() == BST_CHECKED)
        {
            m_proxyConfiguration.proxyMode = ProxyMode::AutoDetect;
        }
        else if (m_bntUsePac.GetCheck() == BST_CHECKED)
        {
            m_proxyConfiguration.proxyMode = ProxyMode::AutoConfig;
        }
        else if (m_bntUseManualProxy.GetCheck() == BST_CHECKED)
        {
            m_proxyConfiguration.proxyMode = ProxyMode::Manual;
        }
        else if (m_bntNoProxy.GetCheck() == BST_CHECKED)
        {
            m_proxyConfiguration.proxyMode = ProxyMode::Direct;
        }
        else
        {
            m_proxyConfiguration.proxyMode = ProxyMode::System;
        }

        CString strAutoConfigUrl;
        m_ecAutoConfigUrl.GetWindowText(strAutoConfigUrl);
        m_proxyConfiguration.autoConfigUrl = Unicode::ToUtf8(strAutoConfigUrl.GetString());

        CString strHttpProxyUrl;
        m_ecHttpProxyUrl.GetWindowText(strHttpProxyUrl);
        m_proxyConfiguration.httpProxyUrl = Unicode::ToUtf8(strHttpProxyUrl.GetString());

        CString strHttpsProxyUrl;
        m_ecHttpsProxyUrl.GetWindowText(strHttpsProxyUrl);
        m_proxyConfiguration.httpsProxyUrl = Unicode::ToUtf8(strHttpsProxyUrl.GetString());
    }
}
