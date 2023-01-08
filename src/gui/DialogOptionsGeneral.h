//
// DialogOptionsGeneral.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include "resource.h"
#include <common/ProxyConfiguration.h>

namespace winpx
{
    class DialogOptionsGeneral : public CPropertyPageImpl<DialogOptionsGeneral>
    {
    public:
        DialogOptionsGeneral(ProxyConfiguration& proxyConfiguration);
        ~DialogOptionsGeneral();

    public:
        static constexpr auto IDD = IDD_OPTIONS_GENERAL;

        BEGIN_MSG_MAP(DialogOptionsGeneral)
            MSG_WM_INITDIALOG(OnInitDialog)
            COMMAND_HANDLER_EX(IDC_GENERAL_NETWORK_MODE, CBN_SELCHANGE, OnControlUpdateNeeded)
            COMMAND_HANDLER_EX(IDC_GENERAL_PROXY_PORT, EN_CHANGE, OnControlUpdateNeeded)
            COMMAND_HANDLER_EX(IDC_GENERAL_ADD_FIREWALL_RULE, BN_CLICKED, OnAddFirewallRule)
            COMMAND_HANDLER_EX(IDC_GENERAL_COPY_PROXY_URL, BN_CLICKED, OnCopyProxyUrl)
            CHAIN_MSG_MAP(CPropertyPageImpl<DialogOptionsGeneral>)
        END_MSG_MAP()

        LRESULT OnInitDialog(HWND hWndCtl, LPARAM lParam);
        void OnControlUpdateNeeded(UINT nNotifyCode, int nID, HWND hWndCtl);
        void OnAddFirewallRule(UINT nNotifyCode, int nID, HWND hWndCtl);
        void OnCopyProxyUrl(UINT nNotifyCode, int nID, HWND hWndCtl);
        int OnSetActive();
        int OnApply();
        void OnHelp();

    private:
        void UpdateGroupPolicy();
        void UpdateControls();
        void LoadSettings();
        void SaveSettings();

    private:
        ProxyConfiguration& m_proxyConfiguration;
        CButton m_btnAutoStartApp;
        CButton m_btnAutoStartProxy;
        CButton m_btnSetEnvironmentVariables;
        CComboBox m_cbNetworkMode;
        CButton m_btnAddFirewallRule;
        CEdit m_ecProxyPort;
        CEdit m_ecWinPxProxyUrl;
        CStatic m_stGroupPolicy;
        bool m_usesNatNetworkingMode = false;
    };
}
