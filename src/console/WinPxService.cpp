//
// WinPxService.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include "pch.h"

#include "WinPxService.h"
#include "WinPxService.tmh"

#include "service_messages.h"
#include <common/ProxyCommandLineParser.h>
#include <win32/Environment.h>
#include <win32/Unicode.h>

namespace winpx
{
    using namespace std::literals::string_view_literals;
    using namespace win32;

    WinPxService::WinPxService()
    {
        DoTraceMessage(WppObject, "%!FUNC!");

        m_serviceName = "WinPX";
    }

    WinPxService::~WinPxService()
    {
        DoTraceMessage(WppObject, "%!FUNC!");
    }

    void WinPxService::Start()
    {
        DoTraceMessage(WppService, "%!FUNC!");

        ParseCommandLineArguments();

        m_proxy = std::make_unique<Proxy>(m_proxyConfiguration);
        m_proxy->Start();

        std::string info;
        info += std::format("Username: {}\r\n", Environment::GetUsername());
        info += std::format("Proxy Port: {}\r\n", m_proxy->GetProxyConfiguration().port);
        info += m_proxy->FormatProxyConfiguration();
        info += m_proxy->FormatProxyUrls();

        if (m_proxy->GetState() != winpx::ProxyState::Running)
        {
            ReportErrorMessage(0, WINPX_ERROR_START_FAILURE, { Unicode::FromUtf8(m_proxy->GetErrorCode().message()) });
            ReportInformationMessage(0, WINPX_PROXY_INFO, { Unicode::FromUtf8(info) });
            throw std::system_error(m_proxy->GetErrorCode());
        }

        ReportInformationMessage(0, WINPX_STARTED);
        ReportInformationMessage(0, WINPX_PROXY_INFO, { Unicode::FromUtf8(info) });
    }

    void WinPxService::Run()
    {
        DoTraceMessage(WppService, "%!FUNC!");

        if (m_proxy)
        {
            m_proxy->WaitForIoShutdown();
        }
    }

    void WinPxService::Stop()
    {
        DoTraceMessage(WppService, "%!FUNC!");

        ReportInformationMessage(0, WINPX_STOPPED);

        if (m_proxy)
        {
            m_proxy->Stop();
            m_proxy.reset();
        }
    }

    void WinPxService::ParseCommandLineArguments()
    {
        DoTraceMessage(WppVerbose, "%!FUNC!");

        ProxyCommandLineParser parser(m_proxyConfiguration);
        parser.Parse(m_arguments);
    }
}
