//
// DialogOptionsStatus.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include "resource.h"
#include <common/HttpProbe.h>
#include <common/Proxy.h>

namespace winpx
{
    class DialogOptionsStatus : public CPropertyPageImpl<DialogOptionsStatus>
    {
    public:
        DialogOptionsStatus(std::shared_ptr<Proxy> proxy);
        ~DialogOptionsStatus();

    public:
        static constexpr auto IDD = IDD_OPTIONS_STATUS;

        BEGIN_MSG_MAP(DialogOptionsStatus)
            MSG_WM_INITDIALOG(OnInitDialog)
            MESSAGE_HANDLER(m_nWmStatusUpdate, OnStatusUpdate)
            CHAIN_MSG_MAP(CPropertyPageImpl<DialogOptionsStatus>)
        END_MSG_MAP()

        LRESULT OnInitDialog(HWND hWndCtl, LPARAM lParam);
        LRESULT OnStatusUpdate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
        void OnHelp();

    private:
        void UpdateControls();
        void ProbeUrls();

        std::string GetResult();
        std::string FormatProxyUrls() const;
        static std::string FormatProxyState(ProxyState proxyState);
        static std::string FormatHttpStatus(HttpStatusCode code);

    private:
        const UINT m_nWmStatusUpdate = WM_APP;
        std::shared_ptr<Proxy> m_proxy;
        HttpProbe m_pacUrl;
        HttpProbe m_httpProxyUrl;
        HttpProbe m_httpsProxyUrl;
        CEdit m_stResult;
    };
}
