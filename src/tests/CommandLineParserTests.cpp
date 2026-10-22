//
// CommandLineParserTests.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include <common/CommandLineParser.h>

#include <common/StringHelper.h>

#include <gtest/gtest.h>

namespace UnitTests
{
    using namespace winpx;
    using namespace std::literals::string_view_literals;

    class CommandLineParserTest : public testing::Test
    {
    protected:
        CommandLineParser m_parser;
    };

    TEST_F(CommandLineParserTest, ParsesBoolOption)
    {
        bool value = false;
        m_parser.AddOption("value", value);

        m_parser.Parse({});
        ASSERT_FALSE(value);

        m_parser.Parse({ "--value" });
        ASSERT_TRUE(value);
    };

    TEST_F(CommandLineParserTest, ParsesStringOption)
    {
        std::string value("default");
        m_parser.AddOption("value", value);

        m_parser.Parse({});
        ASSERT_EQ(value, "default");

        m_parser.Parse({ "--value=abc" });
        ASSERT_EQ(value, "abc");

        m_parser.Parse({ "--value=" });
        ASSERT_EQ(value, "");
    };

    TEST_F(CommandLineParserTest, ParsesCallbackOption)
    {
        int value = 0;
        m_parser.AddOption("value", [&](std::string_view text) { value = ParseDec<uint16_t>(text); });

        m_parser.Parse({});
        ASSERT_EQ(value, 0);

        m_parser.Parse({ "--value=123" });
        ASSERT_EQ(value, 123);
    };

    TEST_F(CommandLineParserTest, ParsesShortBoolOption)
    {
        bool value = false;
        m_parser.AddOption("v", value);

        m_parser.Parse({});
        ASSERT_FALSE(value);

        m_parser.Parse({ "-v" });
        ASSERT_TRUE(value);
    };

    TEST_F(CommandLineParserTest, ParsesShortStringOption)
    {
        std::string value("default");
        m_parser.AddOption("v", value);

        m_parser.Parse({});
        ASSERT_EQ(value, "default");

        m_parser.Parse({ "-v=abc" });
        ASSERT_EQ(value, "abc");

        m_parser.Parse({ "-v=" });
        ASSERT_EQ(value, "");
    };

    TEST_F(CommandLineParserTest, ParsesShortCallbackOption)
    {
        int value = 0;
        m_parser.AddOption("v", [&](std::string_view text) { value = ParseDec<uint16_t>(text); });

        m_parser.Parse({});
        ASSERT_EQ(value, 0);

        m_parser.Parse({ "-v=123" });
        ASSERT_EQ(value, 123);
    };

    TEST_F(CommandLineParserTest, ParsesWindowsBoolOption)
    {
        bool value = false;
        m_parser.AddOption("v", value);

        m_parser.Parse({});
        ASSERT_FALSE(value);

        m_parser.Parse({ "-v" });
        ASSERT_TRUE(value);
    };

    TEST_F(CommandLineParserTest, ParsesWindowsStringOption)
    {
        std::string value("default");
        m_parser.AddOption("v", value);

        m_parser.Parse({});
        ASSERT_EQ(value, "default");

        m_parser.Parse({ "/v=abc" });
        ASSERT_EQ(value, "abc");

        m_parser.Parse({ "/v=" });
        ASSERT_EQ(value, "");
    };

    TEST_F(CommandLineParserTest, ParsesWindowsCallbackOption)
    {
        int value = 0;
        m_parser.AddOption("v", [&](std::string_view text) { value = ParseDec<uint16_t>(text); });

        m_parser.Parse({});
        ASSERT_EQ(value, 0);

        m_parser.Parse({ "/v=123" });
        ASSERT_EQ(value, 123);
    };

    TEST_F(CommandLineParserTest, ThrowsExceptionOnInvalidArguments)
    {
        bool bool_value = false;
        std::string string_value("default");
        m_parser.AddOption("bool", bool_value);
        m_parser.AddOption("string", string_value);

        m_parser.Parse({});
        EXPECT_THROW(m_parser.Parse({ "bla" }), std::runtime_error);
        EXPECT_THROW(m_parser.Parse({ "--none" }), std::runtime_error);
        EXPECT_THROW(m_parser.Parse({ "--bool=" }), std::runtime_error);
        EXPECT_THROW(m_parser.Parse({ "--bool=true" }), std::runtime_error);
        EXPECT_THROW(m_parser.Parse({ "--string" }), std::runtime_error);
    };
}
