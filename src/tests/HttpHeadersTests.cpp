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
    };

    TEST_F(HttpHeadersTest, AddGetContainsWorkCorrectly)
    {
        Add("a"sv, "va"sv);
        Add("b"sv, "vb"sv);
        Add("c"sv, "vc1"sv);
        Add("c"sv, "vc2"sv);

        ASSERT_EQ(GetCount(), 4);

        ASSERT_TRUE(Contains("a"sv));
        ASSERT_TRUE(Contains("b"sv));
        ASSERT_TRUE(Contains("c"sv));
        ASSERT_FALSE(Contains("d"sv));

        ASSERT_EQ(GetValue("a"sv), "va"sv);
        ASSERT_EQ(GetValue("b"sv), "vb"sv);
        ASSERT_EQ(GetValue("c"sv), "vc2"sv);
        ASSERT_FALSE(GetValue("d"sv));

        ASSERT_EQ(GetValues("a"sv), std::vector<std::string_view>{ "va"sv });
        ASSERT_EQ(GetValues("b"sv), std::vector<std::string_view>{ "vb"sv });
        ASSERT_EQ(GetValues("c"sv), (std::vector<std::string_view>{ "vc1"sv, "vc2"sv }));
        ASSERT_TRUE(GetValues("d"sv).empty());
    };

    TEST_F(HttpHeadersTest, NameIsCaseInsensitive)
    {
        Add("aAa"sv, "va"sv);

        ASSERT_TRUE(Contains("aaa"sv));
        ASSERT_TRUE(Contains("AAA"sv));

        ASSERT_EQ(GetValue("aaa"sv), "va"sv);
        ASSERT_EQ(GetValue("AAA"sv), "va"sv);

        ASSERT_EQ(GetValues("aaa"sv), std::vector<std::string_view>{ "va"sv });
        ASSERT_EQ(GetValues("AAA"sv), std::vector<std::string_view>{ "va"sv });
    };
}
