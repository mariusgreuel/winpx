//
// HttpToolsTests.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include <common/HttpTools.h>

#include <gtest/gtest.h>

namespace UnitTests
{
    using namespace winpx;

    TEST(HttpToolsTests, PickProxyFromList)
    {
        ASSERT_EQ(HttpTools::PickProxyFromList("", ""), "");
        ASSERT_EQ(HttpTools::PickProxyFromList(" ", ""), "");
        ASSERT_EQ(HttpTools::PickProxyFromList("127.0.0.1:1111", ""), "127.0.0.1:1111");
        ASSERT_EQ(HttpTools::PickProxyFromList(" 127.0.0.1:1111 ", ""), "127.0.0.1:1111");
        ASSERT_EQ(HttpTools::PickProxyFromList("http=127.0.0.1:1111", ""), "127.0.0.1:1111");
        ASSERT_EQ(HttpTools::PickProxyFromList("127.0.0.1:1111;127.0.0.1:2222", ""), "127.0.0.1:1111");
        ASSERT_EQ(HttpTools::PickProxyFromList(" 127.0.0.1:1111 ; 127.0.0.1:2222 ", ""), "127.0.0.1:1111");
        ASSERT_EQ(HttpTools::PickProxyFromList("http=127.0.0.1:1111;https=127.0.0.1:2222;socks=127.0.0.1:3333", "http://example.com"), "127.0.0.1:1111");
        ASSERT_EQ(HttpTools::PickProxyFromList("http=127.0.0.1:1111;https=127.0.0.1:2222;socks=127.0.0.1:3333", "https://example.com"), "127.0.0.1:2222");
        ASSERT_EQ(HttpTools::PickProxyFromList("HTTP=127.0.0.1:1111;HTTPS=127.0.0.1:2222;SOCKS=127.0.0.1:3333", "http://example.com"), "127.0.0.1:1111");
        ASSERT_EQ(HttpTools::PickProxyFromList("HTTP=127.0.0.1:1111;HTTPS=127.0.0.1:2222;SOCKS=127.0.0.1:3333", "https://example.com"), "127.0.0.1:2222");
        ASSERT_EQ(HttpTools::PickProxyFromList(" http = 127.0.0.1:1111 ; https = 127.0.0.1:2222 ; socks = 127.0.0.1:3333", "http://example.com"), "127.0.0.1:1111");
        ASSERT_EQ(HttpTools::PickProxyFromList(" http = 127.0.0.1:1111 ; https = 127.0.0.1:2222 ; socks = 127.0.0.1:3333", "https://example.com"), "127.0.0.1:2222");
    };
}
