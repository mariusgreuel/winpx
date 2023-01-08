//
// HttpProbe.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include "pch.h"

#include "HttpProbe.h"
#include "HttpProbe.tmh"

namespace winpx
{
    void HttpProbe::Probe(const std::string& url, HWND hWnd, UINT nMessage)
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: url=%!str!", url);

        if (url != m_url)
        {
            m_url = url;
            m_hWnd = hWnd;
            m_nMessage = nMessage;

            try
            {
                m_httpWebRequest.SetUserAgent("WinPX-Diagnostics/1.0");
                m_httpWebRequest.CreateAsync(url, [this](HttpStatusCode statusCode) {
                    m_statusCode = statusCode;
                    PostMessage(m_hWnd, m_nMessage, 0, 0);
                });
            }
            catch (const std::exception& e)
            {
                DoTraceMessage(WppWarning, "Failed to probe URL: %s", e.what());
                m_statusCode = HttpStatusCode::BadRequest;
            }
        }
    }

    bool HttpProbe::IsEmpty() const
    {
        return m_url.empty();
    }

    std::string HttpProbe::GetUrl() const
    {
        return m_url;
    }

    HttpStatusCode HttpProbe::GetStatusCode() const
    {
        return m_statusCode;
    }
}
