//
// HttpMessageTests.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include <common/HttpMessage.h>

#include <common/HttpError.h>

#include <gtest/gtest.h>

namespace UnitTests
{
    using namespace winpx;
    using namespace std::literals::string_view_literals;

    class HttpMessageTest : public testing::Test, public HttpMessage
    {
    protected:
        std::vector<std::string> GetHeaderLines(std::string_view request)
        {
            std::vector<std::string> lines;

            size_t offset = 0;
            std::string_view line;
            while (GetHeaderLine(request, offset, line))
            {
                lines.push_back(std::string(line));
            }

            return lines;
        }
    };

    TEST_F(HttpMessageTest, GetHeaderLine)
    {
        ASSERT_EQ(GetHeaderLines("\r\n"sv), std::vector<std::string>{});
        ASSERT_EQ(GetHeaderLines("Line1\r\n\r\n"sv), (std::vector<std::string>{ "Line1" }));
        ASSERT_EQ(GetHeaderLines("Line1\r\nLine2\r\nLine3\r\n\r\nBla\rBla\nBla"sv), (std::vector<std::string>{ "Line1", "Line2", "Line3" }));
    };

    TEST_F(HttpMessageTest, RejectsInvalidHeaderLines)
    {
        ASSERT_THROW(GetHeaderLines(""), HttpError);
        ASSERT_THROW(GetHeaderLines("\r"), HttpError);
        ASSERT_THROW(GetHeaderLines("\n"), HttpError);
        ASSERT_THROW(GetHeaderLines("\n\r"), HttpError);
        ASSERT_THROW(GetHeaderLines("Line1\r\n"sv), HttpError);
        ASSERT_THROW(GetHeaderLines("Line1\r\r\n"sv), HttpError);
        ASSERT_THROW(GetHeaderLines("Line1\n\r\n"sv), HttpError);
        ASSERT_THROW(GetHeaderLines("Line\n1\r\n\r\n"sv), HttpError);
        ASSERT_THROW(GetHeaderLines("Line\r1\r\n\r\n"sv), HttpError);
    };

    TEST_F(HttpMessageTest, ParseHeader)
    {
        ParseHeader("a:va");
        ParseHeader(" b : vb ");
        ASSERT_EQ(m_headers.GetValue("a"sv), "va");
        ASSERT_EQ(m_headers.GetValue("b"sv), "vb");

        ASSERT_THROW(ParseHeader("No Colon"), HttpError);
    };

    TEST_F(HttpMessageTest, ParseHeaders)
    {
        ParseHeaders("Host: example.com\r\nUser-Agent: WinPX/1.2.3\r\nAccept: */*\r\n\r\n");

        ASSERT_EQ(m_headers.GetValue("Host"sv), "example.com");
        ASSERT_EQ(m_headers.GetValue("User-Agent"sv), "WinPX/1.2.3");
        ASSERT_EQ(m_headers.GetValue("Accept"sv), "*/*");
        ASSERT_FALSE(m_headers.GetValue("Not-There"sv));

        ASSERT_TRUE(m_headers.GetValue("HOST"sv));
        ASSERT_FALSE(m_headers.GetValue("Not-There"sv));
    };
}
