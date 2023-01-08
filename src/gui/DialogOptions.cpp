//
// DialogOptions.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include "pch.h"

#include "DialogOptions.h"
#include "DialogOptions.tmh"

namespace winpx
{
    HWND DialogOptions::m_hWndInstance = nullptr;

    DialogOptions::DialogOptions(std::shared_ptr<Proxy> proxy, ProxyConfiguration& proxyConfiguration, UINT nStartPage) :
        CPropertySheetImpl(IDS_OPTIONS_DIALOG, nStartPage),
        m_dlgGeneral(proxyConfiguration),
        m_dlgGateway(proxyConfiguration),
        m_dlgSecurity(proxyConfiguration),
        m_dlgStatus(proxy),
        m_dlgDiagnostics(proxy),
        m_dlgAbout()
    {
        DoTraceMessage(WppObject, "%!FUNC!");

        m_psh.dwFlags = PSH_USECALLBACK | PSH_NOAPPLYNOW | PSH_NOCONTEXTHELP;

        AddPage(m_dlgGeneral);
        AddPage(m_dlgGateway);
        AddPage(m_dlgSecurity);

        if (proxy)
        {
            AddPage(m_dlgStatus);
            AddPage(m_dlgDiagnostics);
        }

        AddPage(m_dlgAbout);
    }

    DialogOptions::~DialogOptions()
    {
        DoTraceMessage(WppObject, "%!FUNC!");

        m_hWndInstance = nullptr;
    }

    int CALLBACK DialogOptions::PropSheetCallback(HWND hWnd, UINT uMsg, LPARAM lParam)
    {
        if (uMsg == PSCB_PRECREATE)
        {
            auto pDlgTemplate = reinterpret_cast<LPDLGTEMPLATE>(lParam);
            pDlgTemplate->style |= WS_BORDER | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
        }
        else if (uMsg == PSCB_INITIALIZED)
        {
            m_hWndInstance = hWnd;

            CIcon icon;
            icon.LoadIcon(IDI_WINPX);
            ::SendMessage(hWnd, WM_SETICON, ICON_BIG, (LPARAM)icon.Detach());
        }

        return CPropertySheetImpl<DialogOptions>::PropSheetCallback(hWnd, uMsg, lParam);
    }

    void DialogOptions::OnShowWindow(BOOL /* bShow */, UINT /* nStatus */)
    {
        m_Tab.Attach(GetDlgItem(ATL_IDC_TAB_CONTROL));
        CenterWindow();
        SetMsgHandled(FALSE);
    }
}
