//
// DialogOptionsGateway.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include "resource.h"
#include <common/ProxyConfiguration.h>

namespace winpx
{
    class DialogOptionsGateway : public CPropertyPageImpl<DialogOptionsGateway>
    {
    public:
        DialogOptionsGateway(ProxyConfiguration& proxyConfiguration);
        ~DialogOptionsGateway();

    public:
        static constexpr auto IDD = IDD_OPTIONS_GATEWAY;

        BEGIN_MSG_MAP(DialogOptionsGateway)
            MSG_WM_INITDIALOG(OnInitDialog)
            COMMAND_HANDLER_EX(IDC_GATEWAY_USE_SYSTEM_PROXY, BN_CLICKED, OnProxyTypeClicked)
            COMMAND_HANDLER_EX(IDC_GATEWAY_USE_AUTO_DETECT, BN_CLICKED, OnProxyTypeClicked)
            COMMAND_HANDLER_EX(IDC_GATEWAY_USE_AUTO_CONFIG, BN_CLICKED, OnProxyTypeClicked)
            COMMAND_HANDLER_EX(IDC_GATEWAY_USE_MANUAL_PROXY, BN_CLICKED, OnProxyTypeClicked)
            COMMAND_HANDLER_EX(IDC_GATEWAY_NO_PROXY, BN_CLICKED, OnProxyTypeClicked)
            CHAIN_MSG_MAP(CPropertyPageImpl<DialogOptionsGateway>)
        END_MSG_MAP()

        LRESULT OnInitDialog(HWND hWndCtl, LPARAM lParam);
        void OnProxyTypeClicked(UINT nNotifyCode, int nID, HWND hWndCtl);
        int OnApply();
        void OnHelp();

    private:
        void UpdateGroupPolicy();
        void UpdateControls();
        void LoadSettings();
        void SaveSettings();

    private:
        ProxyConfiguration& m_proxyConfiguration;
        CButton m_bntUseSystemProxy;
        CButton m_bntUseWpad;
        CButton m_bntUsePac;
        CButton m_bntUseManualProxy;
        CButton m_bntNoProxy;
        CStatic m_stAutoConfigUrl;
        CEdit m_ecAutoConfigUrl;
        CStatic m_stHttpProxyUrl;
        CEdit m_ecHttpProxyUrl;
        CStatic m_stHttpsProxyUrl;
        CEdit m_ecHttpsProxyUrl;
        CStatic m_stGroupPolicy;
    };
}
