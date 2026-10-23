//
// HttpHeadersTests.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include <common/HttpHeaders.h>

#include <gtest/gtest.h>

namespace UnitTests
{
    using namespace winpx;
    using namespace std::literals::string_view_literals;

    class HttpHeadersTest : public testing::Test, public HttpHeaders
    {
    protected:
        HttpHeadersTest()
        {
            Add("a"sv, "va"sv);
            Add("b"sv, "vb"sv);
            Add("c"sv, "vc1"sv);
            Add("c"sv, "vc2"sv);
            Add("d"sv, "vd1"sv);
            Add("d"sv, "vd2, vd3, vd4 "sv);
            Add("e", "");
            Add("e", "5,, \t,5,");
            Add("aAa"sv, "va"sv);
        }
    };

    TEST_F(HttpHeadersTest, GetCount)
    {
        ASSERT_EQ(GetCount(), 9u);
    };

    TEST_F(HttpHeadersTest, Contains)
    {
        ASSERT_TRUE(Contains("a"sv));
        ASSERT_TRUE(Contains("b"sv));
        ASSERT_TRUE(Contains("c"sv));
        ASSERT_TRUE(Contains("d"sv));
        ASSERT_TRUE(Contains("e"sv));
        ASSERT_FALSE(Contains("x"sv));
    };

    TEST_F(HttpHeadersTest, GetValue)
    {
        ASSERT_EQ(GetValue("a"sv), "va"sv);
        ASSERT_EQ(GetValue("b"sv), "vb"sv);
        ASSERT_EQ(GetValue("c"sv), "vc2"sv);
        ASSERT_EQ(GetValue("d"sv), "vd2, vd3, vd4 "sv);
        ASSERT_FALSE(GetValue("x"sv));
    };

    TEST_F(HttpHeadersTest, GetValues)
    {
        ASSERT_EQ(GetValues("a"sv), std::vector<std::string_view>{ "va"sv });
        ASSERT_EQ(GetValues("b"sv), std::vector<std::string_view>{ "vb"sv });
        ASSERT_EQ(GetValues("c"sv), (std::vector<std::string_view>{ "vc1"sv, "vc2"sv }));
        ASSERT_EQ(GetValues("d"sv), (std::vector<std::string_view>{ "vd1"sv, "vd2, vd3, vd4 "sv }));
        ASSERT_EQ(GetValues("e"sv), (std::vector<std::string_view>{ ""sv, "5,, \t,5,"sv }));
        ASSERT_TRUE(GetValues("x"sv).empty());
    };

    TEST_F(HttpHeadersTest, GetListValues)
    {
        ASSERT_EQ(GetListValues("c"sv), (std::vector<std::string_view>{ "vc1"sv, "vc2"sv }));
        ASSERT_EQ(GetListValues("d"sv), (std::vector<std::string_view>{ "vd1"sv, "vd2"sv, "vd3"sv, "vd4"sv }));
        ASSERT_TRUE(GetListValues("x"sv).empty());
        EXPECT_EQ(GetListValues("e"), (std::vector<std::string_view>{ "", "5", "", "", "5", "" }));
    };

    TEST_F(HttpHeadersTest, Select)
    {
        ASSERT_EQ(Select([](std::string_view name) { return name == "c"sv; }).GetCount(), 2u);
        ASSERT_EQ(Select([](std::string_view name) { return name != "c"sv; }).GetCount(), 7u);
    };

    TEST_F(HttpHeadersTest, NameIsCaseInsensitive)
    {
        ASSERT_TRUE(Contains("aaa"sv));
        ASSERT_TRUE(Contains("AAA"sv));

        ASSERT_EQ(GetValue("aaa"sv), "va"sv);
        ASSERT_EQ(GetValue("AAA"sv), "va"sv);

        ASSERT_EQ(GetValues("aaa"sv), std::vector<std::string_view>{ "va"sv });
        ASSERT_EQ(GetValues("AAA"sv), std::vector<std::string_view>{ "va"sv });
    };
}
