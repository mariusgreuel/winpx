//
// HttpRequestMessageTests.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include <common/HttpRequestMessage.h>

#include "TestHelpers.h"

#include <gtest/gtest.h>

namespace UnitTests
{
    using namespace winpx;
    using namespace winpx::tests;
    using namespace std::literals::string_view_literals;

    class HttpRequestMessageTest : public testing::Test
    {
    protected:
        void Parse(std::string_view message)
        {
            m_request.Parse(message);
            ASSERT_EQ(message, m_request.ToString());
        }

        HttpRequestMessage m_request;
    };

    TEST_F(HttpRequestMessageTest, Parse_Request10Line)
    {
        Parse("GET / HTTP/1.0\r\n\r\n"sv);
        ASSERT_EQ(m_request.GetMethod(), "GET");
        ASSERT_EQ(m_request.GetRequestUri().ToString(), "/");
        ASSERT_EQ(m_request.GetVersion(), 10);
    };

    TEST_F(HttpRequestMessageTest, Parse_Request11Line)
    {
        Parse("GET / HTTP/1.1\r\n\r\n"sv);
        ASSERT_EQ(m_request.GetMethod(), "GET");
        ASSERT_EQ(m_request.GetRequestUri().ToString(), "/");
        ASSERT_EQ(m_request.GetVersion(), 11);
    };

    TEST_F(HttpRequestMessageTest, Parse_RejectsInvalidRequestLine)
    {
        ASSERT_THROW(Parse("GET / http/1.0\r\n\r\n"sv), std::runtime_error);
        ASSERT_THROW(Parse("GET / HTTP/1.1 \n\n"sv), std::runtime_error);
        ASSERT_THROW(Parse("GET / HTTP/0.0\r\n\r\n"sv), std::runtime_error);
        ASSERT_THROW(Parse("GET / HTTP/0.1\r\n\r\n"sv), std::runtime_error);
        ASSERT_THROW(Parse("GET / HTTP/1.2\r\n\r\n"sv), std::runtime_error);
        ASSERT_THROW(Parse("GET / HTTP/1.x\r\n\r\n"sv), std::runtime_error);
        ASSERT_THROW(Parse("GET / HTTP/1.0\n\r\n"sv), std::runtime_error);
    };

    TEST_F(HttpRequestMessageTest, Parse_RequestLineNoHeader)
    {
        Parse("GET http://example.com/ HTTP/1.1\r\n\r\n"sv);
        ASSERT_EQ(m_request.GetHeaders().GetCount(), 0);
    };

    TEST_F(HttpRequestMessageTest, Parse_RequestLineOneHeader)
    {
        Parse("GET http://example.com/ HTTP/1.1\r\nHost: bla\r\n\r\n"sv);
        ASSERT_EQ(m_request.GetHeaders().GetValue("Host"sv), "bla");
    };

    TEST_F(HttpRequestMessageTest, Parse_GetHeaderReturnsLastHeader)
    {
        Parse("GET http://example.com/ HTTP/1.1\r\nAccept: abc\r\nAccept: def\r\n\r\n"sv);
        ASSERT_EQ(m_request.GetHeaders().GetValue("ACCEPT"sv), "def");
    };

    TEST_F(HttpRequestMessageTest, Parse_ParsesGetRequest)
    {
        Parse(NormalizeCrLf(R"(
GET http://example.com/some/path/file.html HTTP/1.1
Host: example.com
User-Agent: WinPX/1.2.3
Accept: */*
Proxy-Connection: Keep-Alive

)"sv));
        ASSERT_EQ(m_request.GetMethod(), "GET");
        ASSERT_EQ(m_request.GetRequestUri().ToString(), "http://example.com/some/path/file.html");
        ASSERT_EQ(m_request.GetVersion(), 11);
        ASSERT_EQ(m_request.GetHeaders().GetValue("Host"sv), "example.com");
        ASSERT_EQ(m_request.GetHeaders().GetValue("User-Agent"sv), "WinPX/1.2.3");
        ASSERT_EQ(m_request.GetHeaders().GetValue("Accept"sv), "*/*");
        ASSERT_EQ(m_request.GetHeaders().GetValue("Proxy-Connection"sv), "Keep-Alive");
    };

    TEST_F(HttpRequestMessageTest, Parse_ParsesPostRequest)
    {
        Parse(NormalizeCrLf(R"(
POST http://example.com/ HTTP/1.1
Host: example.com
User-Agent: WinPX/1.2.3
Accept: */*
Proxy-Connection: Keep-Alive
Content-Length: 27
Content-Type: application/x-www-form-urlencoded

param1=value1&param2=value2)"sv));
        ASSERT_EQ(m_request.GetMethod(), "POST");
        ASSERT_EQ(m_request.GetRequestUri().ToString(), "http://example.com/");
        ASSERT_EQ(m_request.GetHeaders().GetValue("Host"sv), "example.com");
        ASSERT_EQ(m_request.GetHeaders().GetValue("User-Agent"sv), "WinPX/1.2.3");
        ASSERT_EQ(m_request.GetHeaders().GetValue("Accept"sv), "*/*");
        ASSERT_EQ(m_request.GetHeaders().GetValue("Proxy-Connection"sv), "Keep-Alive");
        ASSERT_EQ(m_request.GetHeaders().GetValue("Content-Length"sv), "27");
        ASSERT_EQ(m_request.GetHeaders().GetValue("Content-Type"sv), "application/x-www-form-urlencoded");
    };

    TEST_F(HttpRequestMessageTest, Parse_ParsesConnectRequest)
    {
        Parse(NormalizeCrLf(R"(
CONNECT example.com:443 HTTP/1.1
Host: example.com:443
User-Agent: WinPX/1.2.3
Proxy-Connection: Keep-Alive

)"sv));
        ASSERT_EQ(m_request.GetMethod(), "CONNECT");
        ASSERT_EQ(m_request.GetRequestUri().ToString(), "example.com:443");
        ASSERT_EQ(m_request.GetHeaders().GetValue("Host"sv), "example.com:443");
        ASSERT_EQ(m_request.GetHeaders().GetValue("User-Agent"sv), "WinPX/1.2.3");
        ASSERT_EQ(m_request.GetHeaders().GetValue("Proxy-Connection"sv), "Keep-Alive");
    };
}
