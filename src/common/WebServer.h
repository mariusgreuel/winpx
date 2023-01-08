//
// WebServer.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include "HttpRequestMessage.h"
#include "HttpResponseMessage.h"
#include "ProxyConfiguration.h"
#include "ProxyStatistics.h"

namespace winpx
{
    class WebServer
    {
    public:
        WebServer(const ProxyConfiguration& proxyConfiguration, const ProxyStatistics& proxyStatistics);
        ~WebServer();

        WebServer(const WebServer&) = delete;
        WebServer& operator=(const WebServer&) = delete;

        HttpResponseMessage ProcessRequest(const HttpRequestMessage& request);

    private:
        HttpResponseMessage GetResource(const HttpRequestMessage& request);
        HttpResponseMessage GetStatusPage();
        HttpResponseMessage GetStylesCss();
        HttpResponseMessage GetWinPxPac();
        HttpResponseMessage GetFavicon();
        HttpResponseMessage CreateResponse(std::string_view contentType, std::string content, const std::vector<HttpHeader>& headers = {});

    private:
        static std::string s_favicon;
        const ProxyConfiguration& m_proxyConfiguration;
        const ProxyStatistics& m_proxyStatistics;
    };
}
