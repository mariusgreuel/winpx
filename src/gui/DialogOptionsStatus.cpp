//
// DialogOptionsStatus.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include "pch.h"

#include "DialogOptionsStatus.h"
#include "DialogOptionsStatus.tmh"

#include "OnlineHelp.h"
#include <common/HttpProbe.h>
#include <common/Wsl.h>
#include <win32/Unicode.h>
#include <win32/Win32.h>

namespace winpx
{
    using namespace std::literals::string_view_literals;
    using namespace win32;

    DialogOptionsStatus::DialogOptionsStatus(std::shared_ptr<Proxy> proxy) :
        m_proxy(proxy)
    {
        DoTraceMessage(WppObject, "new DialogOptionsStatus-%p", this);

        EnableHelp();
    }

    DialogOptionsStatus::~DialogOptionsStatus()
    {
        DoTraceMessage(WppObject, "delete DialogOptionsStatus-%p", this);
    }

    LRESULT DialogOptionsStatus::OnInitDialog(HWND /* hWndCtl */, LPARAM /* lParam */)
    {
        DoTraceMessage(WppGui, "%!FUNC!");

        m_stResult.Attach(GetDlgItem(IDC_STATUS_RESULT));

        ProbeUrls();
        UpdateControls();

        return 0;
    }

    LRESULT DialogOptionsStatus::OnStatusUpdate(UINT /* uMsg */, WPARAM /* wParam */, LPARAM /* lParam */, BOOL& bHandled)
    {
        DoTraceMessage(WppGui, "%!FUNC!");

        UpdateControls();

        bHandled = TRUE;
        return 0;
    }

    void DialogOptionsStatus::OnHelp()
    {
        DoTraceMessage(WppGui, "%!FUNC!");

        OnlineHelp::Open("Status"sv);
    }

    void DialogOptionsStatus::UpdateControls()
    {
        DoTraceMessage(WppGui, "%!FUNC!");

        m_stResult.SetWindowText(Unicode::FromUtf8(GetResult()).c_str());
    }

    void DialogOptionsStatus::ProbeUrls()
    {
        if (!m_proxy)
            return;

        auto& proxyConfiguration = m_proxy->GetProxyConfiguration();
        switch (proxyConfiguration.proxyMode)
        {
        case ProxyMode::Default:
        case ProxyMode::System:
        {
            auto systemProxyConfiguration = m_proxy->GetProxyResolver().GetSystemProxyConfiguration();
            if (!systemProxyConfiguration.autoConfigUrl.empty())
            {
                m_pacUrl.Probe(systemProxyConfiguration.autoConfigUrl, m_hWnd, m_nWmStatusUpdate);
            }
            else if (!systemProxyConfiguration.proxy.empty())
            {
                m_httpProxyUrl.Probe(HttpTools::PickProxyFromList(systemProxyConfiguration.proxy, "http:"sv), m_hWnd, m_nWmStatusUpdate);
                m_httpsProxyUrl.Probe(HttpTools::PickProxyFromList(systemProxyConfiguration.proxy, "https:"sv), m_hWnd, m_nWmStatusUpdate);
            }
            break;
        }
        case ProxyMode::Manual:
            m_httpProxyUrl.Probe(proxyConfiguration.httpProxyUrl, m_hWnd, m_nWmStatusUpdate);
            m_httpsProxyUrl.Probe(proxyConfiguration.httpProxyUrl, m_hWnd, m_nWmStatusUpdate);
            break;
        case ProxyMode::AutoConfig:
            m_pacUrl.Probe(proxyConfiguration.autoConfigUrl, m_hWnd, m_nWmStatusUpdate);
            break;
        }
    }

    std::string DialogOptionsStatus::GetResult()
    {
        std::string result;

        if (m_proxy)
        {
            result += std::format("Proxy State: {}\r\n", FormatProxyState(m_proxy->GetState()));
            if (m_proxy->GetState() == ProxyState::Error)
            {
                result += std::format("Proxy Error: {}\r\n", Win32::FormatErrorCode(m_proxy->GetErrorCode()));
            }
            result += m_proxy->FormatProxyConfiguration();
            result += FormatProxyUrls();
            result += std::format("WinPX Proxy URL: {}\r\n", m_proxy->GetProxyConfiguration().GetWinPxProxyUrl());
        }
        else
        {
            result += "State: Stopped";
        }

        if (Wsl::IsVersion2Used())
        {
            if (m_proxy)
            {
                result += std::format("WSL2 Proxy URL: {}\r\n", m_proxy->GetProxyConfiguration().GetWslProxyUrl());
            }

            result += std::format("WSL2 Networking Mode: {}\r\n", Wsl::GetNetworkingMode());

            auto virtualSwitchAddress = Wsl::GetVirtualSwitchAddress();
            if (!virtualSwitchAddress.empty())
            {
                result += std::format("WSL2 Virtual Switch Address: {}\r\n", virtualSwitchAddress);
            }
        }

        return result;
    }

    std::string DialogOptionsStatus::FormatProxyUrls() const
    {
        std::string result;

        if (!m_pacUrl.IsEmpty())
        {
            result += std::format("PAC URL: {}{}\r\n", m_pacUrl.GetUrl(), FormatHttpStatus(m_pacUrl.GetStatusCode()));
        }

        if (!m_httpProxyUrl.IsEmpty())
        {
            result += std::format("HTTP proxy: {}{}\r\n", m_httpProxyUrl.GetUrl(), FormatHttpStatus(m_httpProxyUrl.GetStatusCode()));
        }

        if (!m_httpsProxyUrl.IsEmpty())
        {
            result += std::format("HTTPS proxy: {}{}\r\n", m_httpsProxyUrl.GetUrl(), FormatHttpStatus(m_httpsProxyUrl.GetStatusCode()));
        }

        return result;
    }

    std::string DialogOptionsStatus::FormatProxyState(ProxyState proxyState)
    {
        switch (proxyState)
        {
        case ProxyState::Starting:
            return "Starting";
        case ProxyState::Running:
            return "Running";
        case ProxyState::Stopped:
            return "Stopped";
        case ProxyState::Error:
            return "Error";
        default:
            return "Unknown";
        }
    }

    std::string DialogOptionsStatus::FormatHttpStatus(HttpStatusCode code)
    {
        if (code == HttpStatusCode::Unknown)
        {
            return {};
        }

        return std::format(" (Status: {})", ToReasonPhrase(code));
    }
}
