//
// DialogOptions.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include "DialogOptionsAbout.h"
#include "DialogOptionsDiagnostics.h"
#include "DialogOptionsGateway.h"
#include "DialogOptionsGeneral.h"
#include "DialogOptionsSecurity.h"
#include "DialogOptionsStatus.h"
#include <common/Proxy.h>

namespace winpx
{
    class DialogOptions : public CPropertySheetImpl<DialogOptions>
    {
    public:
        DialogOptions(std::shared_ptr<Proxy> proxy, ProxyConfiguration& proxyConfiguration, UINT nStartPage = 0);
        ~DialogOptions();

        static HWND GetInstance() { return m_hWndInstance; }

    public:
        BEGIN_MSG_MAP(DialogOptions)
            MSG_WM_SHOWWINDOW(OnShowWindow)
            CHAIN_MSG_MAP(CPropertySheetImpl<DialogOptions>)
        END_MSG_MAP()

        static int CALLBACK PropSheetCallback(HWND hWnd, UINT uMsg, LPARAM lParam);
        void OnShowWindow(BOOL bShow, UINT nStatus);

    private:
        static HWND m_hWndInstance;
        CTabCtrl m_Tab;
        DialogOptionsGeneral m_dlgGeneral;
        DialogOptionsGateway m_dlgGateway;
        DialogOptionsSecurity m_dlgSecurity;
        DialogOptionsStatus m_dlgStatus;
        DialogOptionsDiagnostics m_dlgDiagnostics;
        DialogOptionsAbout m_dlgAbout;
    };
}
