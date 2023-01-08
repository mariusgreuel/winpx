//
// DialogOptionsDiagnostics.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include "pch.h"

#include "DialogOptionsDiagnostics.h"
#include "DialogOptionsDiagnostics.tmh"

#include "OnlineHelp.h"
#include <common/HttpWebRequest.h>
#include <common/Tools.h>
#include <win32/Unicode.h>

namespace winpx
{
    using namespace std::literals::string_view_literals;
    using namespace win32;

    DialogOptionsDiagnostics::DialogOptionsDiagnostics(std::shared_ptr<Proxy> proxy) :
        m_proxy(proxy)
    {
        DoTraceMessage(WppObject, "new DialogOptionsDiagnostics-%p", this);

        EnableHelp();
    }

    DialogOptionsDiagnostics::~DialogOptionsDiagnostics()
    {
        DoTraceMessage(WppObject, "delete DialogOptionsDiagnostics-%p", this);
    }

    LRESULT DialogOptionsDiagnostics::OnInitDialog(HWND /* hWndCtl */, LPARAM /* lParam */)
    {
        DoTraceMessage(WppGui, "%!FUNC!");

        m_ecUrl.Attach(GetDlgItem(IDC_DIAGNOSTICS_URL));
        m_stResult.Attach(GetDlgItem(IDC_DIAGNOSTICS_RESULT));

        auto diagnosticUrls = SplitString(m_proxy->GetProxyConfiguration().diagnosticUrls, ';');
        if (diagnosticUrls.size() > 0)
        {
            m_ecUrl.SetWindowText(Unicode::FromUtf8(diagnosticUrls[0]).c_str());
        }

        return 0;
    }

    void DialogOptionsDiagnostics::OnCheck(UINT /* uNotifyCode */, int /* nID */, HWND /* hWnd */)
    {
        DoTraceMessage(WppGui, "%!FUNC!");

        CString strUrl;
        m_ecUrl.GetWindowText(strUrl);
        auto url = Unicode::ToUtf8(strUrl.GetString());

        std::string result;
        result += std::format("Resolving proxy for '{}'...\r\n", url);

        if (m_proxy)
        {
            result += std::format("Resolved proxy: {}\r\n", ResolveProxyForUrl(url));

            result += std::format("Sending GET request to '{}'...\r\n", url);
            HttpWebRequest request;
            request.SetUserAgent("WinPX-Diagnostics/1.0");
            request.SetProxy(m_proxy->GetProxyConfiguration().GetWinPxProxyUrl());
            HttpStatusCode code = request.Create(url);
            result += std::format("HTTP status code: {}\r\n", FormatHttpStatus(code));
            if (request.GetError() == ERROR_SUCCESS)
            {
                result += std::string(80, '-') + "\r\n";
                result += request.QueryRawHeaders();
            }
            else
            {
                result += std::format("ERROR: {}\r\n", request.GetErrorMessage());
            }
        }
        else
        {
            result = "Proxy not running";
        }

        m_stResult.SetWindowText(Unicode::FromUtf8(result).c_str());
    }

    void DialogOptionsDiagnostics::OnHelp()
    {
        DoTraceMessage(WppGui, "%!FUNC!");

        OnlineHelp::Open("Diagnostics"sv);
    }

    std::string DialogOptionsDiagnostics::ResolveProxyForUrl(const std::string& url) const
    {
        if (!m_proxy)
            return {};

        auto result = m_proxy->ResolveProxy(url);
        if (result.direct)
        {
            return "Direct connection";
        }
        else
        {
            return result.host + ":" + std::to_string(result.port);
        }
    }

    std::string DialogOptionsDiagnostics::FormatHttpStatus(HttpStatusCode code)
    {
        return std::format("{} ({})", ToReasonPhrase(code), static_cast<int>(code));
    }
}
