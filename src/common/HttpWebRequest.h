//
// HttpWebRequest.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include "HttpStatusCode.h"
#include "Uri.h"

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace winpx
{
    class HttpWebRequest
    {
    public:
        HttpWebRequest() = default;
        ~HttpWebRequest();

        HttpWebRequest(const HttpWebRequest&) = delete;
        HttpWebRequest& operator=(const HttpWebRequest&) = delete;

    public:
        using Callback = std::function<void(HttpStatusCode)>;

        void SetUserAgent(const std::string& userAgent);
        void SetProxy(const std::string& proxy);

        HttpStatusCode Create(const std::string& url);
        HttpStatusCode CreateAsync(const std::string& url, Callback callback);

        DWORD GetError() const;
        std::string GetErrorMessage() const;

        HttpStatusCode QueryStatusCode();
        std::string QueryRawHeaders();

    private:
        HttpStatusCode Create(const std::string& url, bool async);
        void OpenSession();
        void Connect(LPCWSTR pszServerName, INTERNET_PORT nServerPort);
        void OpenRequest(LPCWSTR pszVerb, LPCWSTR pszObject, bool secure);
        void SendRequest();
        void ReceiveResponseHeaders();
        void ReceiveResponseData();
        void FreeHandles();

        static void CALLBACK StaticAsyncCallback(HINTERNET hInternet, DWORD_PTR dwContext, DWORD dwInternetStatus, LPVOID lpvStatusInformation, DWORD dwStatusInformationLength);
        void AsyncCallback(HINTERNET hInternet, DWORD dwInternetStatus, LPVOID lpvStatusInformation, DWORD dwStatusInformationLength);

    private:
        HINTERNET m_hSession = nullptr;
        HINTERNET m_hConnection = nullptr;
        HINTERNET m_hRequest = nullptr;
        Callback m_callback;
        std::string m_userAgent;
        std::string m_proxy;
        std::string m_proxyUsername;
        std::string m_proxyPassword;
        Uri m_uri;
        bool m_async = false;
        DWORD m_dwLastError = ERROR_SUCCESS;
        HttpStatusCode m_statusCode = HttpStatusCode::Unknown;
        std::vector<std::byte> m_content;
    };
}
