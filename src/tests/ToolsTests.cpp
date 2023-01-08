//
// ToolsTests.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include <common/Tools.h>

#include <gtest/gtest.h>

#include <string_view>

namespace UnitTests
{
    using namespace winpx;
    using namespace std::literals::string_view_literals;

    TEST(ToolsTests, ToLowerChar)
    {
        ASSERT_EQ(ToLower('0'), '0');
        ASSERT_EQ(ToLower('a'), 'a');
        ASSERT_EQ(ToLower('z'), 'z');
        ASSERT_EQ(ToLower('A'), 'a');
        ASSERT_EQ(ToLower('Z'), 'z');
    };

    TEST(ToolsTests, ToUpperChar)
    {
        ASSERT_EQ(ToUpper('0'), '0');
        ASSERT_EQ(ToUpper('a'), 'A');
        ASSERT_EQ(ToUpper('z'), 'Z');
        ASSERT_EQ(ToUpper('A'), 'A');
        ASSERT_EQ(ToUpper('Z'), 'Z');
    };

    TEST(ToolsTests, ToLowerText)
    {
        ASSERT_EQ(ToLower(""), "");
        ASSERT_EQ(ToLower(" 0123aaBBccDD#XyZ$"), " 0123aabbccdd#xyz$");
    };

    TEST(ToolsTests, ToUpperText)
    {
        ASSERT_EQ(ToUpper(""), "");
        ASSERT_EQ(ToUpper(" 0123aaBBccDD#XyZ$"), " 0123AABBCCDD#XYZ$");
    };

    TEST(ToolsTests, IEquals)
    {
        ASSERT_TRUE(IEquals(""sv, ""sv));
        ASSERT_TRUE(IEquals("0"sv, "0"sv));
        ASSERT_TRUE(IEquals("abc"sv, "abc"sv));
        ASSERT_TRUE(IEquals("ABC"sv, "ABC"sv));
        ASSERT_TRUE(IEquals("aBc"sv, "abc"sv));
        ASSERT_TRUE(IEquals("abc"sv, "aBc"sv));
        ASSERT_TRUE(IEquals("AbC"sv, "ABC"sv));
        ASSERT_TRUE(IEquals("ABC"sv, "AbC"sv));

        ASSERT_FALSE(IEquals("a"sv, ""sv));
        ASSERT_FALSE(IEquals(""sv, "a"sv));
        ASSERT_FALSE(IEquals("a"sv, "b"sv));
        ASSERT_FALSE(IEquals("A"sv, "B"sv));
        ASSERT_FALSE(IEquals("a"sv, "aa"sv));
        ASSERT_FALSE(IEquals("aa"sv, "a"sv));
        ASSERT_FALSE(IEquals("bb"sv, "bbb"sv));
        ASSERT_FALSE(IEquals("bbb"sv, "bb"sv));
    };

    TEST(ToolsTests, IStartsWith)
    {
        ASSERT_TRUE(IStartsWith(""sv, ""sv));
        ASSERT_TRUE(IStartsWith("a"sv, ""sv));
        ASSERT_TRUE(IStartsWith("a"sv, "a"sv));
        ASSERT_TRUE(IStartsWith("abc"sv, "a"sv));
        ASSERT_TRUE(IStartsWith("abc"sv, "abc"sv));
        ASSERT_TRUE(IStartsWith("abcdef"sv, "abc"sv));
        ASSERT_TRUE(IStartsWith("A"sv, "a"sv));
        ASSERT_TRUE(IStartsWith("ABC"sv, "a"sv));
        ASSERT_TRUE(IStartsWith("AbC"sv, "aBc"sv));
        ASSERT_TRUE(IStartsWith("ABCDEF"sv, "abc"sv));
        ASSERT_TRUE(IStartsWith("a"sv, "A"sv));
        ASSERT_TRUE(IStartsWith("abc"sv, "A"sv));
        ASSERT_TRUE(IStartsWith("aBc"sv, "AbC"sv));
        ASSERT_TRUE(IStartsWith("abcdef"sv, "ABC"sv));

        ASSERT_FALSE(IStartsWith(""sv, "a"sv));
        ASSERT_FALSE(IStartsWith("a"sv, "aa"sv));
        ASSERT_FALSE(IStartsWith("aa"sv, "aab"sv));
        ASSERT_FALSE(IStartsWith("a"sv, "b"sv));
        ASSERT_FALSE(IStartsWith("abcdef"sv, "xbcx"sv));
    };

    TEST(ToolsTests, Trim)
    {
        ASSERT_EQ(Trim(""sv, ' '), ""sv);
        ASSERT_EQ(Trim("bla"sv, ' '), "bla"sv);
        ASSERT_EQ(Trim("bla bla"sv, ' '), "bla bla"sv);
        ASSERT_EQ(Trim(" bla bla "sv, ' '), "bla bla"sv);
        ASSERT_EQ(Trim("   bla bla   "sv, ' '), "bla bla"sv);
    };

    TEST(ToolsTests, GetSubstring)
    {
        size_t offset = 0;
        std::string_view token;
        ASSERT_FALSE(GetSubstring(""sv, offset, token, ' '));

        offset = 0;
        ASSERT_TRUE(GetSubstring("bla"sv, offset, token, ' ') && token == "bla" && offset == 3);

        offset = 0;
        ASSERT_TRUE(GetSubstring("  bla  "sv, offset, token, ' ') && token == "bla" && offset == 6);
        ASSERT_FALSE(GetSubstring("  bla  "sv, offset, token, ' ') && offset == 7);

        offset = 0;
        ASSERT_TRUE(GetSubstring("a bb  ccc   dddd "sv, offset, token, ' ') && token == "a" && offset == 2);
        ASSERT_TRUE(GetSubstring("a bb  ccc   dddd "sv, offset, token, ' ') && token == "bb" && offset == 5);
        ASSERT_TRUE(GetSubstring("a bb  ccc   dddd "sv, offset, token, ' ') && token == "ccc" && offset == 10);
        ASSERT_TRUE(GetSubstring("a bb  ccc   dddd "sv, offset, token, ' ') && token == "dddd" && offset == 17);
        ASSERT_FALSE(GetSubstring("a  bb ccc   dddd "sv, offset, token, ' ') && offset == 7);
    };

    TEST(ToolsTests, SplitString)
    {
        ASSERT_EQ(SplitString(""sv, ' '), std::vector<std::string_view>{});
        ASSERT_EQ(SplitString("a"sv, ' '), std::vector{ "a"sv });
        ASSERT_EQ(SplitString(" a "sv, ' '), std::vector{ "a"sv });
        ASSERT_EQ(SplitString("  a  "sv, ' '), std::vector{ "a"sv });
        ASSERT_EQ(SplitString("aa"sv, ' '), std::vector{ "aa"sv });
        ASSERT_EQ(SplitString("a b"sv, ' '), (std::vector{ "a"sv, "b"sv }));
        ASSERT_EQ(SplitString("a b c"sv, ' '), (std::vector{ "a"sv, "b"sv, "c"sv }));
        ASSERT_EQ(SplitString(" a b c "sv, ' '), (std::vector{ "a"sv, "b"sv, "c"sv }));
        ASSERT_EQ(SplitString("   aaa   bbb   ccc   "sv, ' '), (std::vector{ "aaa"sv, "bbb"sv, "ccc"sv }));

        ASSERT_EQ(SplitString("a,b,c,d"sv, ','), (std::vector{ "a"sv, "b"sv, "c"sv, "d"sv }));
    };

    TEST(ToolsTests, ParseDec)
    {
        ASSERT_EQ(ParseDec("0"sv), 0);
        ASSERT_EQ(ParseDec("1"sv), 1);
        ASSERT_EQ(ParseDec("12345"sv), 12345);
    };

    TEST(ToolsTests, ParseHex)
    {
        ASSERT_EQ(ParseHex("0"sv), 0);
        ASSERT_EQ(ParseHex("1"sv), 1);
        ASSERT_EQ(ParseHex("12345"sv), 0x12345);
        ASSERT_EQ(ParseHex("123aBc"sv), 0x123ABC);
    };

    TEST(ToolsTests, ParseDecRejectsInvalidInput)
    {
        ASSERT_THROW(ParseDec(""sv), std::invalid_argument);
        ASSERT_THROW(ParseDec("xyz"sv), std::invalid_argument);
        ASSERT_THROW(ParseDec("xyz123"sv), std::invalid_argument);
        ASSERT_THROW(ParseDec("123xyz456"sv), std::invalid_argument);
        ASSERT_THROW(ParseDec<uint16_t>("65536"sv), std::out_of_range);
        ASSERT_THROW(ParseDec<size_t>("99999999999999999999"sv), std::out_of_range);
    };

    TEST(ToolsTests, ParseHexRejectsInvalidInput)
    {
        ASSERT_THROW(ParseHex(""sv), std::invalid_argument);
        ASSERT_THROW(ParseHex("xyz"sv), std::invalid_argument);
        ASSERT_THROW(ParseHex("xyz123"sv), std::invalid_argument);
        ASSERT_THROW(ParseHex("123x"sv), std::invalid_argument);
        ASSERT_THROW(ParseHex<uint16_t>("AAAAA"sv), std::out_of_range);
        ASSERT_THROW(ParseHex<size_t>("FFFFFFFFFFFFFFFFF"sv), std::out_of_range);
    };
}
