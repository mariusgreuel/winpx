//
// DialogOptionsDiagnostics.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include "resource.h"
#include <common/Proxy.h>

namespace winpx
{
    class DialogOptionsDiagnostics : public CPropertyPageImpl<DialogOptionsDiagnostics>
    {
    public:
        DialogOptionsDiagnostics(std::shared_ptr<Proxy> proxy);
        ~DialogOptionsDiagnostics();

    public:
        static constexpr auto IDD = IDD_OPTIONS_DIAGNOSTICS;

        BEGIN_MSG_MAP(DialogOptionsDiagnostics)
            MSG_WM_INITDIALOG(OnInitDialog)
            COMMAND_ID_HANDLER_EX(IDC_DIAGNOSTICS_CHECK, OnCheck)
            CHAIN_MSG_MAP(CPropertyPageImpl<DialogOptionsDiagnostics>)
        END_MSG_MAP()

        LRESULT OnInitDialog(HWND hWndCtl, LPARAM lParam);
        void OnCheck(UINT uNotifyCode, int nID, HWND hWnd);
        void OnHelp();

    private:
        std::string ResolveProxyForUrl(const std::string& url) const;
        static std::string FormatHttpStatus(HttpStatusCode code);

    private:
        std::shared_ptr<Proxy> m_proxy;
        CEdit m_ecUrl;
        CStatic m_stResult;
    };
}
