//
// HttpProbe.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include "HttpStatusCode.h"
#include "HttpWebRequest.h"

namespace winpx
{
    class HttpProbe
    {
    public:
        void Probe(const std::string& url, HWND hWnd, UINT nMessage);
        bool IsEmpty() const;
        std::string GetUrl() const;
        HttpStatusCode GetStatusCode() const;

    private:
        HttpWebRequest m_httpWebRequest;
        std::string m_url;
        HWND m_hWnd = nullptr;
        UINT m_nMessage = 0;
        HttpStatusCode m_statusCode = HttpStatusCode::Unknown;
    };
}
