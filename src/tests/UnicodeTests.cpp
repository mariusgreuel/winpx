//
// UnicodeTests.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include <win32/Unicode.h>

#include <gtest/gtest.h>

namespace UnitTests
{
    using namespace win32;

    TEST(UnicodeTests, ConvertsUtf8ToWide)
    {
        EXPECT_EQ(L"M\u00FCnchen", Unicode::FromUtf8("M\xC3\xBCnchen"));
    }

    TEST(UnicodeTests, RoundTripsUtf8)
    {
        const std::string expected = "Gr\xC3\xBC\xC3\x9F\x65";

        EXPECT_EQ(expected, Unicode::ToUtf8(Unicode::FromUtf8(expected)));
    }
}
