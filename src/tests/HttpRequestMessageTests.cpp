//
// HttpRequestMessageTests.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include <common/HttpRequestMessage.h>

#include <common/HttpError.h>
#include <common/HttpTools.h>

#include <gtest/gtest.h>

namespace UnitTests
{
    using namespace winpx;
    using namespace std::literals::string_view_literals;

    class HttpRequestMessageTest : public testing::Test
    {
    protected:
        void Parse(std::string_view message)
        {
            auto normalizedMessage = HttpTools::NormalizeCrLf(message);
            request.Parse(normalizedMessage);
            ASSERT_EQ(normalizedMessage, request.ToString());
        }

        HttpRequestMessage request;
    };

    TEST_F(HttpRequestMessageTest, ParsesRequestLine)
    {
        Parse("ABC http://example.com/some/path/file.html HTTP/3.4\r\n\r\n"sv);
        ASSERT_EQ(request.GetMethod(), "ABC");
        ASSERT_EQ(request.GetRequestUri(), "http://example.com/some/path/file.html");
        ASSERT_TRUE(request.IsHttpVersion(3, 4));
        ASSERT_FALSE(request.IsHttpVersion(1, 2));
    };

    TEST_F(HttpRequestMessageTest, ParsesRequestLineNoHeader)
    {
        Parse("GET http://example.com/ HTTP/1.1\r\n\r\n"sv);
        ASSERT_EQ(request.GetHeaders().GetCount(), 0);
    };

    TEST_F(HttpRequestMessageTest, ParsesRequestLineOneHeader)
    {
        Parse("GET http://example.com/ HTTP/1.1\r\nHost: bla\r\n\r\n"sv);
        ASSERT_EQ(request.GetHeaders().GetValue("Host"sv), "bla");
    };

    TEST_F(HttpRequestMessageTest, GetHeaderReturnsLastHeader)
    {
        Parse("GET http://example.com/ HTTP/1.1\r\nAccept: abc\r\nAccept: def\r\n\r\n"sv);
        ASSERT_EQ(request.GetHeaders().GetValue("ACCEPT"sv), "def");
    };

    TEST_F(HttpRequestMessageTest, ParsesGetRequest)
    {
        Parse(R"(
GET http://example.com/some/path/file.html HTTP/1.1
Host: example.com
User-Agent: WinPX/1.2.3
Accept: */*
Proxy-Connection: Keep-Alive

)"sv);
        ASSERT_EQ(request.GetMethod(), "GET");
        ASSERT_EQ(request.GetRequestUri(), "http://example.com/some/path/file.html");
        ASSERT_TRUE(request.IsHttpVersion(1, 1));
        ASSERT_EQ(request.GetHeaders().GetValue("Host"sv), "example.com");
        ASSERT_EQ(request.GetHeaders().GetValue("User-Agent"sv), "WinPX/1.2.3");
        ASSERT_EQ(request.GetHeaders().GetValue("Accept"sv), "*/*");
        ASSERT_EQ(request.GetHeaders().GetValue("Proxy-Connection"sv), "Keep-Alive");
    };

    TEST_F(HttpRequestMessageTest, ParsesPostRequest)
    {
        Parse(R"(
POST http://example.com/ HTTP/1.1
Host: example.com
User-Agent: WinPX/1.2.3
Accept: */*
Proxy-Connection: Keep-Alive
Content-Length: 27
Content-Type: application/x-www-form-urlencoded

param1=value1&param2=value2)"sv);
        ASSERT_EQ(request.GetMethod(), "POST");
        ASSERT_EQ(request.GetRequestUri(), "http://example.com/");
        ASSERT_TRUE(request.IsHttpVersion(1, 1));
        ASSERT_EQ(request.GetHeaders().GetValue("Host"sv), "example.com");
        ASSERT_EQ(request.GetHeaders().GetValue("User-Agent"sv), "WinPX/1.2.3");
        ASSERT_EQ(request.GetHeaders().GetValue("Accept"sv), "*/*");
        ASSERT_EQ(request.GetHeaders().GetValue("Proxy-Connection"sv), "Keep-Alive");
        ASSERT_EQ(request.GetHeaders().GetValue("Content-Length"sv), "27");
        ASSERT_EQ(request.GetHeaders().GetValue("Content-Type"sv), "application/x-www-form-urlencoded");
    };

    TEST_F(HttpRequestMessageTest, ParsesConnectRequest)
    {
        Parse(R"(
CONNECT example.com:443 HTTP/1.1
Host: example.com:443
User-Agent: WinPX/1.2.3
Proxy-Connection: Keep-Alive

)"sv);
        ASSERT_EQ(request.GetMethod(), "CONNECT");
        ASSERT_EQ(request.GetRequestUri(), "example.com:443");
        ASSERT_TRUE(request.IsHttpVersion(1, 1));
        ASSERT_EQ(request.GetHeaders().GetValue("Host"sv), "example.com:443");
        ASSERT_EQ(request.GetHeaders().GetValue("User-Agent"sv), "WinPX/1.2.3");
        ASSERT_EQ(request.GetHeaders().GetValue("Proxy-Connection"sv), "Keep-Alive");
    };

    TEST_F(HttpRequestMessageTest, RejectsMalformedRequestLine)
    {
        ASSERT_THROW(Parse("GET / HTTP/1.x\n\n"sv), HttpError);
        ASSERT_THROW(Parse("GET / HTTP/1.1 extra\n\n"sv), HttpError);
    };
}
