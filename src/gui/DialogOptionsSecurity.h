//
// DialogOptionsSecurity.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include "resource.h"
#include <common/ProxyConfiguration.h>

namespace winpx
{
    class DialogOptionsSecurity : public CPropertyPageImpl<DialogOptionsSecurity>
    {
    public:
        DialogOptionsSecurity(ProxyConfiguration& proxyConfiguration);
        ~DialogOptionsSecurity();

    public:
        static constexpr auto IDD = IDD_OPTIONS_SECURITY;

        BEGIN_MSG_MAP(DialogOptionsSecurity)
            MSG_WM_INITDIALOG(OnInitDialog)
            COMMAND_HANDLER_EX(IDC_SECURITY_ALLOW_BASIC, BN_CLICKED, OnControlUpdateNeeded)
            COMMAND_HANDLER_EX(IDC_SECURITY_REQUIRE_LOCAL_AUTH, BN_CLICKED, OnControlUpdateNeeded)
            COMMAND_HANDLER_EX(IDC_SECURITY_SECRET, EN_CHANGE, OnControlUpdateNeeded)
            COMMAND_HANDLER_EX(IDC_SECURITY_REFRESH_SECRET, BN_CLICKED, OnRefreshWinPxSecret)
            CHAIN_MSG_MAP(CPropertyPageImpl<DialogOptionsSecurity>)
        END_MSG_MAP()

        LRESULT OnInitDialog(HWND hWndCtl, LPARAM lParam);
        void OnControlUpdateNeeded(UINT nNotifyCode, int nID, HWND hWndCtl);
        void OnRefreshWinPxSecret(UINT nNotifyCode, int nID, HWND hWndCtl);
        int OnApply();
        void OnHelp();

    private:
        void UpdateGroupPolicy();
        void UpdateControls();
        void LoadSettings();
        void SaveSettings();

    private:
        ProxyConfiguration& m_proxyConfiguration;
        CButton m_bntAllowNegotiate;
        CButton m_bntAllowNtlm;
        CButton m_bntAllowDigest;
        CButton m_bntAllowBasic;
        CStatic m_stGatewayUsername;
        CEdit m_ecGatewayUsername;
        CStatic m_stGatewayPassword;
        CEdit m_ecGatewayPassword;
        CButton m_bntRequireLocalAuthentication;
        CStatic m_stWinPxSecret;
        CEdit m_ecWinPxSecret;
        CButton m_bntRefreshSecret;
        CStatic m_stGroupPolicy;
    };
}
