//
// HttpResponseMessageTests.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include <common/HttpResponseMessage.h>

#include "TestHelpers.h"

#include <gtest/gtest.h>

namespace UnitTests
{
    using namespace winpx;
    using namespace winpx::tests;
    using namespace std::literals::string_view_literals;

    class HttpResponseMessageTest : public testing::Test
    {
    protected:
        void Parse(std::string_view message)
        {
            auto normalizedMessage = NormalizeCrLf(message);
            m_response.Parse(normalizedMessage);
            ASSERT_EQ(normalizedMessage, m_response.ToString());
        }

        HttpResponseMessage m_response;
    };

    TEST_F(HttpResponseMessageTest, Parse_ParsesMessage)
    {
        Parse(R"(
HTTP/1.1 200 OK
Server: Apache/2.4.1 (Unix)
Content-Length: 15
Content-Type: text/html; charset=utf-8

<html></html>
)"sv);
        ASSERT_EQ(m_response.GetVersion(), 11);
        ASSERT_EQ(m_response.GetStatusCode(), HttpStatusCode::OK);
        ASSERT_EQ(m_response.GetReasonPhrase(), "OK");
        ASSERT_EQ(m_response.GetHeaders().GetValue("Server"sv), "Apache/2.4.1 (Unix)");
        ASSERT_EQ(m_response.GetHeaders().GetValue("Content-Length"sv), "15");
        ASSERT_EQ(m_response.GetHeaders().GetValue("Content-Type"sv), "text/html; charset=utf-8");
        ASSERT_EQ(m_response.GetBody(), "<html></html>\r\n");
    };

    TEST_F(HttpResponseMessageTest, Parse_RejectsMalformedStatusCode)
    {
        ASSERT_THROW(Parse("HTTP/1.1 20 OK\r\n\r\n"), std::runtime_error);
        ASSERT_THROW(Parse("HTTP/1.1 200x OK\r\n\r\n"), std::runtime_error);
    };
}
