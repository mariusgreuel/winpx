//
// WebServer.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include <pch.h>

#include "WebServer.h"
#include "WebServer.tmh"

#include "HttpError.h"
#include "Tools.h"
#include "Version.h"

namespace winpx
{
    using namespace std::literals::string_view_literals;

    std::string WebServer::s_favicon = R"(<?xml version="1.0" encoding="UTF-8"?>
<svg id="favicon" version="1.1" width="16" height="16" viewBox="0 0 4.2333333 4.2333333" xmlns="http://www.w3.org/2000/svg" xmlns:svg="http://www.w3.org/2000/svg">
  <path d="m 2.1166667,0 q 0.291455,0 0.5622396,0.07648112 0.2707844,0.07441406 0.506429,0.2129069 0.2356445,0.13642578 0.4278809,0.33072915 0.1943033,0.19223634 0.3307291,0.42788083 0.1384929,0.2356446 0.2129069,0.5064291 0.076481,0.2707845 0.076481,0.5622396 0,0.291455 -0.076481,0.5622396 -0.074414,0.2707844 -0.2129069,0.506429 -0.1364258,0.2356445 -0.3307291,0.4299479 -0.1922364,0.1922363 -0.4278809,0.3307291 -0.2356446,0.1364259 -0.506429,0.212907 -0.2707846,0.074414 -0.5622396,0.074414 -0.2914551,0 -0.5622396,-0.074414 Q 1.2836426,4.0824382 1.047998,3.9460123 0.81235351,3.8075195 0.61805012,3.6152832 0.4258138,3.4209798 0.28732096,3.1853353 0.15089518,2.9496907 0.07441406,2.6809733 0,2.4101888 0,2.1166667 0,1.8252116 0.07441406,1.5544271 0.15089518,1.2836426 0.28732096,1.047998 0.4258138,0.81235351 0.61805012,0.62011717 0.81235351,0.4258138 1.047998,0.28938802 1.2836426,0.15089518 1.55236,0.07648112 1.8231445,0 2.1166667,0 Z M 3.788916,1.3229166 Q 3.7103678,1.155485 3.5987467,1.0087239 3.4871257,0.85989582 3.3506999,0.73793944 3.2142741,0.61598307 3.0551106,0.52089844 2.8959473,0.4258138 2.7223145,0.36586914 2.7967285,0.469222 2.8566731,0.58497719 q 0.059945,0.11575525 0.10542,0.23977863 0.047543,0.12195644 0.080615,0.24804698 0.033073,0.1260904 0.057878,0.2501138 z M 3.96875,2.1166667 Q 3.96875,1.8417481 3.892269,1.5875 H 3.1419271 q 0.016536,0.1322916 0.024804,0.2645834 0.00827,0.1302246 0.00827,0.2645833 0,0.1343587 -0.00827,0.2666504 -0.00827,0.1302245 -0.024804,0.2625162 H 3.8922689 Q 3.96875,2.3915853 3.96875,2.1166667 Z M 2.1166667,3.96875 q 0.1012858,0 0.1881022,-0.055811 Q 2.3936529,3.857128 2.4680665,3.7661778 2.5424805,3.6752278 2.6003581,3.5615392 2.6603028,3.4457845 2.705778,3.3279623 2.7512533,3.21014 2.7822591,3.100586 2.813265,2.9910319 2.8298015,2.9104166 H 1.4035319 q 0.016536,0.080615 0.047543,0.1901694 0.031006,0.109554 0.076481,0.2273763 0.045475,0.1178222 0.1033529,0.2335774 0.059945,0.1136881 0.1343587,0.2046386 0.074414,0.090951 0.1612305,0.1467612 Q 2.015381,3.96875 2.1166667,3.96875 Z M 2.8752766,2.6458333 q 0.016537,-0.1322917 0.024805,-0.2625162 0.010335,-0.1322917 0.010335,-0.2666504 0,-0.1343587 -0.010335,-0.2645833 Q 2.8918131,1.7197916 2.8752766,1.5875 h -1.51722 q -0.016536,0.1322916 -0.026872,0.2645834 -0.00827,0.1302246 -0.00827,0.2645833 0,0.1343587 0.00827,0.2666504 0.010336,0.1302245 0.026872,0.2625162 z M 0.26458333,2.1166667 q 0,0.2749186 0.0764811,0.5291666 H 1.0914063 q -0.016536,-0.1322917 -0.024805,-0.2625162 -0.00827,-0.1322917 -0.00827,-0.2666504 0,-0.1343587 0.00827,-0.2645833 Q 1.0748694,1.7197916 1.0914059,1.5875 H 0.34106445 Q 0.26458333,1.8417481 0.26458333,2.1166667 Z M 2.1166667,0.26458333 q -0.1012859,0 -0.1901692,0.0558106 Q 1.839681,0.37620443 1.765267,0.46715495 1.6908529,0.55810547 1.6309082,0.67386069 1.5730306,0.78754882 1.5275553,0.90537113 1.4820801,1.0231934 1.4510742,1.1327474 1.4200683,1.2423014 1.4035319,1.3229166 H 2.8298015 Q 2.813265,1.2423014 2.7822591,1.1327474 2.7512533,1.0231934 2.705778,0.90537113 2.6603028,0.78754882 2.6003581,0.67386069 2.5424805,0.55810547 2.4680665,0.46715495 2.3936524,0.37620443 2.3047689,0.32039388 2.2179525,0.26458333 2.1166667,0.26458333 Z M 1.5110189,0.36586914 Q 1.3373861,0.4258138 1.1782226,0.52089844 1.0190593,0.61598307 0.88263344,0.73793944 0.74620769,0.85989582 0.63458657,1.0087239 0.52296549,1.155485 0.44441732,1.3229166 H 1.1327474 Q 1.1575521,1.1988932 1.190625,1.0728028 1.2236979,0.94671226 1.2691732,0.82475582 1.3167155,0.70073244 1.3766601,0.58497719 1.4366048,0.469222 1.5110189,0.36586914 Z M 0.44441732,2.9104166 Q 0.52296549,3.0778483 0.63458657,3.2266764 0.74620769,3.3734375 0.88263344,3.4953939 1.0190593,3.6173502 1.1782226,3.7124349 1.3373861,3.8075195 1.5110189,3.8674642 1.4366048,3.7641113 1.3766601,3.6483561 1.3167155,3.5326009 1.2691732,3.4106445 1.2236979,3.2866211 1.190625,3.1605306 1.1575521,3.0344401 1.1327474,2.9104166 Z M 2.7223145,3.8674642 Q 2.8959473,3.8075195 3.0551106,3.7124349 3.2142741,3.6173502 3.3506999,3.4953939 3.4871257,3.3734375 3.5987467,3.2266764 3.7103678,3.0778483 3.788916,2.9104166 H 3.100586 Q 3.075781,3.0344401 3.042708,3.1605306 3.009635,3.2866211 2.962093,3.4106445 2.916618,3.5326009 2.856673,3.6483561 2.796728,3.7641113 2.7223145,3.8674642 Z" style="fill:#000000;fill-opacity:1;stroke-width:0.500001" />
</svg>
)";

    WebServer::WebServer(const ProxyConfiguration& proxyConfiguration, const ProxyStatistics& proxyStatistics) :
        m_proxyConfiguration(proxyConfiguration),
        m_proxyStatistics(proxyStatistics)
    {
        DoTraceMessage(WppObject, "%!FUNC!");
    }

    WebServer::~WebServer()
    {
        DoTraceMessage(WppObject, "%!FUNC!");
    }

    HttpResponseMessage WebServer::ProcessRequest(const HttpRequestMessage& request)
    {
        DoTraceMessage(WppTrace, "%!FUNC!: uri='%!str!'", request.GetRequestUri());

        try
        {
            if (IEquals(request.GetMethod(), "GET"sv))
            {
                return GetResource(request);
            }
            else if (IEquals(request.GetMethod(), "HEAD"sv))
            {
                auto response = GetResource(request);
                response.GetHeaders().Add("Content-Length"sv, std::to_string(response.GetBody().size()));
                response.SetBody({});
                return response;
            }
            else
            {
                throw HttpError(HttpStatusCode::MethodNotAllowed);
            }
        }
        catch (const HttpError& e)
        {
            return HttpResponseMessage(e.GetStatusCode(), e.what());
        }
    }

    HttpResponseMessage WebServer::GetResource(const HttpRequestMessage& request)
    {
        Uri target(request.GetRequestUri());
        if (target.path == "/"sv)
        {
            return GetStatusPage();
        }
        else if (target.path == "/styles.css"sv)
        {
            return GetStylesCss();
        }
        else if (target.path == "/winpx.pac"sv)
        {
            return GetWinPxPac();
        }
        else if (target.path == "/favicon.ico"sv)
        {
            return GetFavicon();
        }
        else
        {
            throw HttpError(HttpStatusCode::NotFound);
        }
    }

    HttpResponseMessage WebServer::GetStatusPage()
    {
        auto content = std::format(
            R"(<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <title>WinPX</title>
    <link rel="stylesheet" href="styles.css">
</head>
<body>
    <header>
        <h1>WinPX V{}.{}.{}</h1>
    </header>
    <main>
        <h2>Status</h2>
        <p>Active client connections: {}</p>
        <p>Total client connections: {}</p>
        <p>Total client bytes: {} sent, {} received</p>
        <p>Total upstream bytes: {} sent, {} received</p>
    </main>
    <footer>
        <p>Copyright (c) 2021 Marius Greuel. All rights reserved.</p>
    </footer>
</body>
</html>
)",
            VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH,
            m_proxyStatistics.activeConnections.load(),
            m_proxyStatistics.totalConnections.load(),
            m_proxyStatistics.totalClientBytesSent.load(),
            m_proxyStatistics.totalClientBytesReceived.load(),
            m_proxyStatistics.totalUpstreamBytesSent.load(),
            m_proxyStatistics.totalUpstreamBytesReceived.load());

        return CreateResponse("text/html"sv, std::move(content));
    }

    HttpResponseMessage WebServer::GetStylesCss()
    {
        static const char* css = R"(/* WinPX */
html {
    font-family: "Segoe UI", Arial, sans-serif;
}

body {
    max-width: 720px;
    margin: 1rem auto;
    display: grid;
    color: #111;
}

header {
}

main {
}

footer {
    border-top: 1px solid #ccc;
    text-align: center;
}
)";

        return CreateResponse("text/css"sv, css);
    }

    HttpResponseMessage WebServer::GetWinPxPac()
    {
        auto content = std::format(
            R"(// WinPX proxy configuration script
function FindProxyForURL(url, host) {{
  return 'PROXY 127.0.0.1:{}';
}}
)",
            m_proxyConfiguration.port);

        return CreateResponse("application/x-ns-proxy-autoconfig"sv, std::move(content), { { "Cache-Control"sv, "no-cache"sv } });
    }

    HttpResponseMessage WebServer::GetFavicon()
    {
        return CreateResponse("image/svg+xml"sv, s_favicon);
    }

    HttpResponseMessage WebServer::CreateResponse(std::string_view contentType, std::string content, const std::vector<HttpHeader>& headers)
    {
        HttpResponseMessage response(HttpStatusCode::OK);
        response.GetHeaders().Add("Server"sv, std::format("WinPX/{}.{}.{} (Windows)", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH));

        for (const auto& header : headers)
        {
            response.GetHeaders().Add(header.name, header.value);
        }

        response.GetHeaders().Add("Content-Type", contentType);
        response.SetBody(std::move(content));
        return response;
    }
}
