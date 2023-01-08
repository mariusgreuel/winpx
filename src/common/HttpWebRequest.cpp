//
// HttpWebRequest.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include <pch.h>

#include "HttpWebRequest.h"
#include "HttpWebRequest.tmh"

#include "Tools.h"
#include <win32/Unicode.h>
#include <win32/Win32.h>
#include <win32/Win32Error.h>

namespace winpx
{
    using namespace std::literals::string_view_literals;
    using namespace win32;

    HttpWebRequest::~HttpWebRequest()
    {
        FreeHandles();
    }

    void HttpWebRequest::SetUserAgent(const std::string& userAgent)
    {
        m_userAgent = userAgent;
    }

    void HttpWebRequest::SetProxy(const std::string& proxy)
    {
        Uri uri(proxy);
        m_proxy = std::format("http://{}:{}", uri.GetHost(), uri.GetPort());
        m_proxyUsername = uri.GetUsername();
        m_proxyPassword = uri.GetPassword();
    }

    HttpStatusCode HttpWebRequest::Create(const std::string& url)
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: url=%!str!", url);

        return Create(url, false);
    }

    HttpStatusCode HttpWebRequest::CreateAsync(const std::string& url, Callback callback)
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: url=%!str!", url);

        m_callback = callback;
        return Create(url, true);
    }

    HttpStatusCode HttpWebRequest::Create(const std::string& url, bool async)
    {
        try
        {
            m_async = async;

            m_uri.Parse(url);

            OpenSession();
            Connect(Unicode::FromUtf8(m_uri.GetHost()).c_str(), m_uri.GetPort());
            OpenRequest(L"GET", Unicode::FromUtf8(m_uri.GetPathAndQuery()).c_str(), IEquals(m_uri.GetScheme(), "https"sv));
            SendRequest();

            if (m_async)
            {
                return HttpStatusCode::Unknown;
            }
            else
            {
                ReceiveResponseHeaders();
                ReceiveResponseData();
                return QueryStatusCode();
            }
        }
        catch (const Win32Error& e)
        {
            DoTraceMessage(WppVerbose, "Failed to create web request: %s", e.what());
            m_dwLastError = e.code().value();
            return QueryStatusCode();
        }
    }

    DWORD HttpWebRequest::GetError() const
    {
        return m_dwLastError;
    }

    std::string HttpWebRequest::GetErrorMessage() const
    {
        return Win32::FormatErrorCode(m_dwLastError);
    }

    void HttpWebRequest::OpenSession()
    {
        DoTraceMessage(WppVerbose, "%!FUNC!");

        DWORD dwFlags = m_async ? WINHTTP_FLAG_ASYNC : 0;
        HINTERNET hSession = WinHttpOpen(
            m_userAgent.empty() ? nullptr : Unicode::FromUtf8(m_userAgent).c_str(),
            m_proxy.empty() ? WINHTTP_ACCESS_TYPE_NO_PROXY : WINHTTP_ACCESS_TYPE_NAMED_PROXY,
            m_proxy.empty() ? WINHTTP_NO_PROXY_NAME : Unicode::FromUtf8(m_proxy).c_str(),
            WINHTTP_NO_PROXY_BYPASS,
            dwFlags);
        if (hSession == nullptr)
        {
            DWORD dwError = GetLastError();
            DoTraceMessage(WppVerbose, "WinHttpOpen failed: %!WINERROR!", dwError);
            throw Win32Error(dwError, "WinHttpOpen failed.");
        }

        m_hSession = hSession;
    }

    void HttpWebRequest::Connect(LPCWSTR pszServerName, INTERNET_PORT nServerPort)
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: pszServerName='%S', nServerPort=%d", pszServerName, nServerPort);

        HINTERNET hConnection = WinHttpConnect(
            m_hSession,
            pszServerName,
            nServerPort,
            0);
        if (hConnection == nullptr)
        {
            DWORD dwError = GetLastError();
            DoTraceMessage(WppVerbose, "WinHttpConnect failed: %!WINERROR!", dwError);
            throw Win32Error(dwError, "WinHttpConnect failed.");
        }

        m_hConnection = hConnection;
    }

    void HttpWebRequest::OpenRequest(LPCWSTR pszVerb, LPCWSTR pszObject, bool secure)
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: pszVerb='%S', pszObject='%S'", pszVerb, pszObject);

        DWORD dwFlags = WINHTTP_FLAG_REFRESH;
        if (secure)
        {
            dwFlags |= WINHTTP_FLAG_SECURE;
        }

        HINTERNET hRequest = WinHttpOpenRequest(
            m_hConnection,
            pszVerb,
            pszObject,
            nullptr,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            dwFlags);
        if (hRequest == nullptr)
        {
            DWORD dwError = GetLastError();
            DoTraceMessage(WppVerbose, "WinHttpOpenRequest failed: %!WINERROR!", dwError);
            throw Win32Error(dwError, "WinHttpOpenRequest failed.");
        }

        DWORD dwPolicy = WINHTTP_AUTOLOGON_SECURITY_LEVEL_HIGH;
        if (!WinHttpSetOption(hRequest, WINHTTP_OPTION_AUTOLOGON_POLICY, &dwPolicy, sizeof(dwPolicy)))
        {
            DWORD dwError = GetLastError();
            DoTraceMessage(WppError, "Failed to set auto logon policy: %!WINERROR!", dwError);
            throw Win32Error(dwError, "Failed to set auto logon policy.");
        }

        if (!m_proxyUsername.empty())
        {
            WinHttpSetCredentials(
                hRequest,
                WINHTTP_AUTH_TARGET_PROXY,
                WINHTTP_AUTH_SCHEME_BASIC,
                Unicode::FromUtf8(m_proxyUsername).c_str(),
                Unicode::FromUtf8(m_proxyPassword).c_str(),
                nullptr);
        }

        if (m_async)
        {
            DWORD_PTR dwContext = reinterpret_cast<DWORD_PTR>(this);
            if (!WinHttpSetOption(hRequest, WINHTTP_OPTION_CONTEXT_VALUE, &dwContext, sizeof(dwContext)))
            {
                DWORD dwError = GetLastError();
                DoTraceMessage(WppError, "Failed to set context value: %!WINERROR!", dwError);
                throw Win32Error(dwError, "Failed to set context value.");
            }

            if (WinHttpSetStatusCallback(hRequest, StaticAsyncCallback, WINHTTP_CALLBACK_FLAG_ALL_COMPLETIONS, 0) == WINHTTP_INVALID_STATUS_CALLBACK)
            {
                DWORD dwError = GetLastError();
                DoTraceMessage(WppError, "Failed to set status callback: %!WINERROR!", dwError);
                throw Win32Error(dwError, "Failed to set status callback.");
            }
        }

        m_hRequest = hRequest;
    }

    void HttpWebRequest::SendRequest()
    {
        DoTraceMessage(WppVerbose, "%!FUNC!");

        if (!WinHttpSendRequest(
                m_hRequest,
                WINHTTP_NO_ADDITIONAL_HEADERS,
                0,
                WINHTTP_NO_REQUEST_DATA,
                0,
                0,
                0))
        {
            DWORD dwError = GetLastError();
            DoTraceMessage(WppVerbose, "WinHttpSendRequest failed: %!WINERROR!", dwError);
            throw Win32Error(dwError, "WinHttpSendRequest failed.");
        }
    }

    void HttpWebRequest::ReceiveResponseHeaders()
    {
        DoTraceMessage(WppVerbose, "%!FUNC!");

        if (!WinHttpReceiveResponse(m_hRequest, nullptr))
        {
            DWORD dwError = GetLastError();
            DoTraceMessage(WppVerbose, "WinHttpReceiveResponse failed: %!WINERROR!", dwError);
            throw Win32Error(dwError, "WinHttpReceiveResponse failed.");
        }
    }

    void HttpWebRequest::ReceiveResponseData()
    {
        DoTraceMessage(WppVerbose, "%!FUNC!");

        while (true)
        {
            DWORD dwNumberOfBytesAvailable = 0;
            if (!WinHttpQueryDataAvailable(m_hRequest, &dwNumberOfBytesAvailable))
            {
                DWORD dwError = GetLastError();
                DoTraceMessage(WppVerbose, "WinHttpQueryDataAvailable failed: %!WINERROR!", dwError);
                throw Win32Error(dwError, "WinHttpQueryDataAvailable failed.");
            }

            if (dwNumberOfBytesAvailable == 0)
                break;

            std::vector<std::byte> buffer(dwNumberOfBytesAvailable);

            DWORD dwNumberOfBytesRead = 0;
            if (!WinHttpReadData(m_hRequest, buffer.data(), dwNumberOfBytesAvailable, &dwNumberOfBytesRead))
            {
                DWORD dwError = GetLastError();
                DoTraceMessage(WppVerbose, "WinHttpReadData failed: %!WINERROR!", dwError);
                throw Win32Error(dwError, "WinHttpReadData failed.");
            }

            m_content.insert(m_content.end(), buffer.begin(), buffer.begin() + dwNumberOfBytesRead);
        }
    }

    HttpStatusCode HttpWebRequest::QueryStatusCode()
    {
        DoTraceMessage(WppVerbose, "%!FUNC!");

        DWORD dwStatusCode = 0;
        DWORD dwSize = sizeof(dwStatusCode);
        if (!WinHttpQueryHeaders(m_hRequest,
                WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX,
                &dwStatusCode,
                &dwSize,
                WINHTTP_NO_HEADER_INDEX))
        {
            DWORD dwError = GetLastError();
            DoTraceMessage(WppVerbose, "WinHttpQueryHeaders failed: %!WINERROR!", dwError);
            return HttpStatusCode::BadRequest;
        }

        return static_cast<HttpStatusCode>(dwStatusCode);
    }

    std::string HttpWebRequest::QueryRawHeaders()
    {
        DoTraceMessage(WppVerbose, "%!FUNC!");

        DWORD dwSize = 0;
        WinHttpQueryHeaders(m_hRequest,
            WINHTTP_QUERY_RAW_HEADERS_CRLF,
            WINHTTP_HEADER_NAME_BY_INDEX,
            NULL,
            &dwSize,
            WINHTTP_NO_HEADER_INDEX);


        std::wstring result(dwSize, '\0');
        if (!WinHttpQueryHeaders(m_hRequest,
                WINHTTP_QUERY_RAW_HEADERS_CRLF,
                WINHTTP_HEADER_NAME_BY_INDEX,
                result.data(),
                &dwSize,
                WINHTTP_NO_HEADER_INDEX))
        {
            DWORD dwError = GetLastError();
            DoTraceMessage(WppVerbose, "WinHttpQueryHeaders failed: %!WINERROR!", dwError);
            return std::string();
        }

        result.resize(dwSize);
        return Unicode::ToUtf8(result);
    }

    void HttpWebRequest::StaticAsyncCallback(HINTERNET hInternet, DWORD_PTR dwContext, DWORD dwInternetStatus, LPVOID lpvStatusInformation, DWORD dwStatusInformationLength)
    {
        auto request = reinterpret_cast<HttpWebRequest*>(dwContext);
        if (request != nullptr)
        {
            request->AsyncCallback(hInternet, dwInternetStatus, lpvStatusInformation, dwStatusInformationLength);
        }
    }

    void HttpWebRequest::AsyncCallback(HINTERNET /* hInternet */, DWORD dwInternetStatus, LPVOID /* lpvStatusInformation */, DWORD /* dwStatusInformationLength */)
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: dwInternetStatus=0x%08X", dwInternetStatus);

        try
        {
            if (dwInternetStatus == WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE)
            {
                ReceiveResponseHeaders();
            }
            else if (dwInternetStatus == WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE)
            {
                m_statusCode = QueryStatusCode();
                if (m_callback)
                {
                    m_callback(m_statusCode);
                }
            }
        }
        catch (const Win32Error& e)
        {
            DoTraceMessage(WppWarning, "Callback failed: %s", e.what());
            if (m_callback)
            {
                m_callback(m_statusCode);
            }
        }
    }

    void HttpWebRequest::FreeHandles()
    {
        if (m_hRequest != nullptr)
        {
            WinHttpCloseHandle(m_hRequest);
            m_hRequest = nullptr;
        }

        if (m_hConnection != nullptr)
        {
            WinHttpCloseHandle(m_hConnection);
            m_hConnection = nullptr;
        }

        if (m_hSession != nullptr)
        {
            WinHttpCloseHandle(m_hSession);
            m_hSession = nullptr;
        }
    }
}
