//
// HttpResponseMessageTests.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include <common/HttpResponseMessage.h>

#include <common/HttpError.h>
#include <common/HttpTools.h>

#include <gtest/gtest.h>

namespace UnitTests
{
    using namespace winpx;
    using namespace std::literals::string_view_literals;

    class HttpResponseMessageTest : public testing::Test
    {
    protected:
        void Parse(std::string_view message)
        {
            auto normalizedMessage = HttpTools::NormalizeCrLf(message);
            response.Parse(normalizedMessage);
            ASSERT_EQ(normalizedMessage, response.ToString());
        }

        HttpResponseMessage response;
    };

    TEST_F(HttpResponseMessageTest, ParsesMessage)
    {
        Parse(R"(
HTTP/1.1 200 OK
Server: Apache/2.4.1 (Unix)
Content-Length: 15
Content-Type: text/html; charset=utf-8

<html></html>
)"sv);
        ASSERT_TRUE(response.IsHttpVersion(1, 1));
        ASSERT_EQ(response.GetStatusCode(), HttpStatusCode::OK);
        ASSERT_EQ(response.GetReasonPhrase(), "OK");
        ASSERT_EQ(response.GetHeaders().GetValue("Server"sv), "Apache/2.4.1 (Unix)");
        ASSERT_EQ(response.GetHeaders().GetValue("Content-Length"sv), "15");
        ASSERT_EQ(response.GetHeaders().GetValue("Content-Type"sv), "text/html; charset=utf-8");
        ASSERT_EQ(response.GetBody(), "<html></html>\r\n");
    };

    TEST_F(HttpResponseMessageTest, RejectsMalformedStatusCode)
    {
        ASSERT_THROW(Parse("HTTP/1.1 20 OK\r\n\r\n"), HttpError);
        ASSERT_THROW(Parse("HTTP/1.1 200x OK\r\n\r\n"), HttpError);
    };
}
