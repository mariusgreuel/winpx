//
// HttpHelperTests.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include <common/HttpHelper.h>

#include <gtest/gtest.h>

namespace UnitTests
{
    using namespace winpx;

    TEST(HttpHelperTests, PickProxyFromList)
    {
        ASSERT_EQ(PickProxyFromList("", ""), "");
        ASSERT_EQ(PickProxyFromList(" ", ""), "");
        ASSERT_EQ(PickProxyFromList("127.0.0.1:1111", ""), "127.0.0.1:1111");
        ASSERT_EQ(PickProxyFromList(" 127.0.0.1:1111 ", ""), "127.0.0.1:1111");
        ASSERT_EQ(PickProxyFromList("http=127.0.0.1:1111", ""), "127.0.0.1:1111");
        ASSERT_EQ(PickProxyFromList("127.0.0.1:1111;127.0.0.1:2222", ""), "127.0.0.1:1111");
        ASSERT_EQ(PickProxyFromList(" 127.0.0.1:1111 ; 127.0.0.1:2222 ", ""), "127.0.0.1:1111");
        ASSERT_EQ(PickProxyFromList("http=127.0.0.1:1111;https=127.0.0.1:2222;socks=127.0.0.1:3333", "http://example.com"), "127.0.0.1:1111");
        ASSERT_EQ(PickProxyFromList("http=127.0.0.1:1111;https=127.0.0.1:2222;socks=127.0.0.1:3333", "https://example.com"), "127.0.0.1:2222");
        ASSERT_EQ(PickProxyFromList("HTTP=127.0.0.1:1111;HTTPS=127.0.0.1:2222;SOCKS=127.0.0.1:3333", "http://example.com"), "127.0.0.1:1111");
        ASSERT_EQ(PickProxyFromList("HTTP=127.0.0.1:1111;HTTPS=127.0.0.1:2222;SOCKS=127.0.0.1:3333", "https://example.com"), "127.0.0.1:2222");
        ASSERT_EQ(PickProxyFromList(" http = 127.0.0.1:1111 ; https = 127.0.0.1:2222 ; socks = 127.0.0.1:3333", "http://example.com"), "127.0.0.1:1111");
        ASSERT_EQ(PickProxyFromList(" http = 127.0.0.1:1111 ; https = 127.0.0.1:2222 ; socks = 127.0.0.1:3333", "https://example.com"), "127.0.0.1:2222");
    };

    TEST(HttpHelperTests, RedactHead_KeepsNonSensitiveHeaders)
    {
        ASSERT_EQ(RedactHead(""), "");
        ASSERT_EQ(RedactHead("ABC\r\nUser-Agent: WinPX\r\nXYZ\r\n\r\n"), "ABC\r\nUser-Agent: WinPX\r\nXYZ");
    };

    TEST(HttpHelperTests, RedactHead_RedactsSensitiveHeaders)
    {
#ifndef _DEBUG
        ASSERT_EQ(RedactHead("ABC\r\nCookie: secret\r\nXYZ\r\n\r\n"), "ABC\r\nCookie: [REDACTED]\r\nXYZ");
        ASSERT_EQ(RedactHead("ABC\r\nWWW-Authenticate: secret\r\nXYZ\r\n\r\n"), "ABC\r\nWWW-Authenticate: [REDACTED]\r\nXYZ");
        ASSERT_EQ(RedactHead("ABC\r\nProxy-Authenticate: secret\r\nXYZ\r\n\r\n"), "ABC\r\nProxy-Authenticate: [REDACTED]\r\nXYZ");
        ASSERT_EQ(RedactHead("ABC\r\nProxy-Authorization: secret\r\nXYZ\r\n\r\n"), "ABC\r\nProxy-Authorization: [REDACTED]\r\nXYZ");
        ASSERT_EQ(RedactHead("ABC\r\nX-Api-Key: secret\r\nXYZ\r\n\r\n"), "ABC\r\nX-Api-Key: [REDACTED]\r\nXYZ");
#else
        ASSERT_EQ(RedactHead("ABC\r\nCookie: secret\r\nXYZ\r\n\r\n"), "ABC\r\nCookie: secret\r\nXYZ");
        ASSERT_EQ(RedactHead("ABC\r\nWWW-Authenticate: secret\r\nXYZ\r\n\r\n"), "ABC\r\nWWW-Authenticate: secret\r\nXYZ");
        ASSERT_EQ(RedactHead("ABC\r\nProxy-Authenticate: secret\r\nXYZ\r\n\r\n"), "ABC\r\nProxy-Authenticate: secret\r\nXYZ");
        ASSERT_EQ(RedactHead("ABC\r\nProxy-Authorization: secret\r\nXYZ\r\n\r\n"), "ABC\r\nProxy-Authorization: secret\r\nXYZ");
        ASSERT_EQ(RedactHead("ABC\r\nX-Api-Key: secret\r\nXYZ\r\n\r\n"), "ABC\r\nX-Api-Key: secret\r\nXYZ");
#endif
    };
}
