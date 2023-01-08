//
// CommandLineParserTests.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include <common/CommandLineParser.h>

#include <common/Tools.h>

#include <gtest/gtest.h>

namespace UnitTests
{
    using namespace winpx;
    using namespace std::literals::string_view_literals;

    class CommandLineParserTest : public testing::Test, public CommandLineParser
    {
    };

    TEST_F(CommandLineParserTest, ParsesBoolOption)
    {
        bool value = false;
        AddOption("value", value);

        Parse({});
        ASSERT_FALSE(value);

        Parse({ "--value" });
        ASSERT_TRUE(value);
    };

    TEST_F(CommandLineParserTest, ParsesStringOption)
    {
        std::string value("default");
        AddOption("value", value);

        Parse({});
        ASSERT_EQ(value, "default");

        Parse({ "--value=abc" });
        ASSERT_EQ(value, "abc");

        Parse({ "--value=" });
        ASSERT_EQ(value, "");
    };

    TEST_F(CommandLineParserTest, ParsesCallbackOption)
    {
        int value = 0;
        AddOption("value", [&](std::string_view text) { value = ParseDec<uint16_t>(text); });

        Parse({});
        ASSERT_EQ(value, 0);

        Parse({ "--value=123" });
        ASSERT_EQ(value, 123);
    };

    TEST_F(CommandLineParserTest, ParsesShortBoolOption)
    {
        bool value = false;
        AddOption("v", value);

        Parse({});
        ASSERT_FALSE(value);

        Parse({ "-v" });
        ASSERT_TRUE(value);
    };

    TEST_F(CommandLineParserTest, ParsesShortStringOption)
    {
        std::string value("default");
        AddOption("v", value);

        Parse({});
        ASSERT_EQ(value, "default");

        Parse({ "-v=abc" });
        ASSERT_EQ(value, "abc");

        Parse({ "-v=" });
        ASSERT_EQ(value, "");
    };

    TEST_F(CommandLineParserTest, ParsesShortCallbackOption)
    {
        int value = 0;
        AddOption("v", [&](std::string_view text) { value = ParseDec<uint16_t>(text); });

        Parse({});
        ASSERT_EQ(value, 0);

        Parse({ "-v=123" });
        ASSERT_EQ(value, 123);
    };

    TEST_F(CommandLineParserTest, ParsesWindowsBoolOption)
    {
        bool value = false;
        AddOption("v", value);

        Parse({});
        ASSERT_FALSE(value);

        Parse({ "-v" });
        ASSERT_TRUE(value);
    };

    TEST_F(CommandLineParserTest, ParsesWindowsStringOption)
    {
        std::string value("default");
        AddOption("v", value);

        Parse({});
        ASSERT_EQ(value, "default");

        Parse({ "/v=abc" });
        ASSERT_EQ(value, "abc");

        Parse({ "/v=" });
        ASSERT_EQ(value, "");
    };

    TEST_F(CommandLineParserTest, ParsesWindowsCallbackOption)
    {
        int value = 0;
        AddOption("v", [&](std::string_view text) { value = ParseDec<uint16_t>(text); });

        Parse({});
        ASSERT_EQ(value, 0);

        Parse({ "/v=123" });
        ASSERT_EQ(value, 123);
    };

    TEST_F(CommandLineParserTest, ThrowsExceptionOnInvalidArguments)
    {
        bool bool_value = false;
        std::string string_value("default");
        AddOption("bool", bool_value);
        AddOption("string", string_value);

        Parse({});
        EXPECT_THROW(Parse({ "bla" }), std::runtime_error);
        EXPECT_THROW(Parse({ "--none" }), std::runtime_error);
        EXPECT_THROW(Parse({ "--bool=" }), std::runtime_error);
        EXPECT_THROW(Parse({ "--bool=true" }), std::runtime_error);
        EXPECT_THROW(Parse({ "--string" }), std::runtime_error);
    };
}
