//
// DialogOptionsGeneral.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include "pch.h"

#include "DialogOptionsGeneral.h"
#include "DialogOptionsGeneral.tmh"

#include "Firewall.h"
#include "OnlineHelp.h"
#include <common/Wsl.h>
#include <win32/Clipboard.h>
#include <win32/GroupPolicy.h>
#include <win32/Unicode.h>

namespace winpx
{
    using namespace std::literals::string_view_literals;
    using namespace win32;

    DialogOptionsGeneral::DialogOptionsGeneral(ProxyConfiguration& proxyConfiguration) :
        m_proxyConfiguration(proxyConfiguration),
        m_usesNatNetworkingMode(Wsl::UsesNatNetworkingMode())
    {
        DoTraceMessage(WppObject, "new DialogOptionsGeneral-%p", this);

        EnableHelp();
    }

    DialogOptionsGeneral::~DialogOptionsGeneral()
    {
        DoTraceMessage(WppObject, "delete DialogOptionsGeneral-%p", this);
    }

    LRESULT DialogOptionsGeneral::OnInitDialog(HWND /* hWndCtl */, LPARAM /* lParam */)
    {
        DoTraceMessage(WppGui, "%!FUNC!");

        m_btnAutoStartApp.Attach(GetDlgItem(IDC_GENERAL_AUTO_START_APP));
        m_btnAutoStartProxy.Attach(GetDlgItem(IDC_GENERAL_AUTO_START_PROXY));
        m_btnSetEnvironmentVariables.Attach(GetDlgItem(IDC_GENERAL_SET_ENVIRONMENT));
        m_cbNetworkMode.Attach(GetDlgItem(IDC_GENERAL_NETWORK_MODE));
        m_btnAddFirewallRule.Attach(GetDlgItem(IDC_GENERAL_ADD_FIREWALL_RULE));
        m_ecProxyPort.Attach(GetDlgItem(IDC_GENERAL_PROXY_PORT));
        m_ecWinPxProxyUrl.Attach(GetDlgItem(IDC_GENERAL_WINPX_PROXY_URL));
        m_stGroupPolicy.Attach(GetDlgItem(IDC_GROUP_POLICY));

        m_btnAddFirewallRule.SendMessage(BCM_SETSHIELD, 0, TRUE);

        m_cbNetworkMode.AddString(L"Loopback");
        m_cbNetworkMode.AddString(L"Loopback + WSL");
        m_cbNetworkMode.AddString(L"Any");

        LoadSettings();
        UpdateGroupPolicy();
        UpdateControls();

        return 0;
    }

    void DialogOptionsGeneral::OnControlUpdateNeeded(UINT nNotifyCode, int nID, HWND /* hWndCtl */)
    {
        DoTraceMessage(WppGui, "%!FUNC!: nNotifyCode=%u, nID=%d", nNotifyCode, nID);

        SaveSettings();
        UpdateControls();
    }

    void DialogOptionsGeneral::OnAddFirewallRule(UINT nNotifyCode, int nID, HWND /* hWndCtl */)
    {
        DoTraceMessage(WppGui, "%!FUNC!: nNotifyCode=%u, nID=%d", nNotifyCode, nID);

        CString strApplication;
        (void)strApplication.LoadString(IDS_WINPX);

        std::error_code ec = Firewall::AddWslExemption(m_proxyConfiguration.port);
        if (ec)
        {
            std::string message = std::format("Failed to add firewall rule: {}", ec.message());
            MessageBoxW(Unicode::FromUtf8(message).c_str(), strApplication, MB_OK | MB_ICONWARNING);
        }
        else
        {
            MessageBoxW(L"Firewall rule added successfully.", strApplication, MB_OK | MB_ICONINFORMATION);
        }
    }

    void DialogOptionsGeneral::OnCopyProxyUrl(UINT nNotifyCode, int nID, HWND /* hWndCtl */)
    {
        DoTraceMessage(WppGui, "%!FUNC!: nNotifyCode=%u, nID=%d", nNotifyCode, nID);

        Clipboard::CopyToClipboard(m_proxyConfiguration.GetWinPxProxyUrl().c_str());
    }

    int DialogOptionsGeneral::OnSetActive()
    {
        DoTraceMessage(WppGui, "%!FUNC!");

        UpdateControls();
        return 0;
    }

    int DialogOptionsGeneral::OnApply()
    {
        DoTraceMessage(WppGui, "%!FUNC!");

        SaveSettings();
        return PSNRET_NOERROR;
    }

    void DialogOptionsGeneral::OnHelp()
    {
        DoTraceMessage(WppGui, "%!FUNC!");

        OnlineHelp::Open("General"sv);
    }

    void DialogOptionsGeneral::UpdateGroupPolicy()
    {
        DoTraceMessage(WppGui, "%!FUNC!");

        if (GroupPolicy::GetValue("AutoStartApp", false))
        {
            m_btnAutoStartApp.EnableWindow(FALSE);
            m_btnAutoStartApp.SetCheck(BST_UNCHECKED);
            m_stGroupPolicy.ShowWindow(SW_SHOW);
        }

        if (GroupPolicy::GetValue("AutoStartProxy", false))
        {
            m_btnAutoStartProxy.EnableWindow(FALSE);
            m_btnAutoStartProxy.SetCheck(BST_UNCHECKED);
            m_stGroupPolicy.ShowWindow(SW_SHOW);
        }

        int port = GroupPolicy::GetValue("ProxyPort", -1);
        if (port >= 0)
        {
            CString strPort;
            strPort.Format(L"%d", port);
            m_ecProxyPort.EnableWindow(FALSE);
            m_ecProxyPort.SetWindowText(strPort);
            m_stGroupPolicy.ShowWindow(SW_SHOW);
        }
    }

    void DialogOptionsGeneral::UpdateControls()
    {
        DoTraceMessage(WppGui, "%!FUNC!");

        bool useWslNat = m_usesNatNetworkingMode && (static_cast<NetworkMode>(m_cbNetworkMode.GetCurSel()) == NetworkMode::LoopbackAndWsl);
        m_btnAddFirewallRule.ShowWindow(useWslNat ? SW_SHOW : SW_HIDE);

        m_ecWinPxProxyUrl.SetWindowText(Unicode::FromUtf8(m_proxyConfiguration.GetWinPxProxyUrl()).c_str());
    }

    void DialogOptionsGeneral::LoadSettings()
    {
        m_btnAutoStartApp.SetCheck(m_proxyConfiguration.autoStartApp ? BST_CHECKED : BST_UNCHECKED);
        m_btnAutoStartProxy.SetCheck(m_proxyConfiguration.autoStartProxy ? BST_CHECKED : BST_UNCHECKED);
        m_btnSetEnvironmentVariables.SetCheck(m_proxyConfiguration.setEnvironmentVariables ? BST_CHECKED : BST_UNCHECKED);
        m_cbNetworkMode.SetCurSel(static_cast<int>(m_proxyConfiguration.networkMode));
        m_ecProxyPort.SetWindowText(std::to_wstring(m_proxyConfiguration.port).c_str());
        m_ecWinPxProxyUrl.SetWindowText(Unicode::FromUtf8(m_proxyConfiguration.GetWinPxProxyUrl()).c_str());
    }

    void DialogOptionsGeneral::SaveSettings()
    {
        m_proxyConfiguration.autoStartApp = m_btnAutoStartApp.GetCheck() == BST_CHECKED;
        m_proxyConfiguration.autoStartProxy = m_btnAutoStartProxy.GetCheck() == BST_CHECKED;
        m_proxyConfiguration.setEnvironmentVariables = m_btnSetEnvironmentVariables.GetCheck() == BST_CHECKED;
        m_proxyConfiguration.networkMode = static_cast<NetworkMode>(m_cbNetworkMode.GetCurSel());

        CString strServerPort;
        m_ecProxyPort.GetWindowText(strServerPort);
        m_proxyConfiguration.port = static_cast<uint16_t>(_wtoi(strServerPort));
    }
}
