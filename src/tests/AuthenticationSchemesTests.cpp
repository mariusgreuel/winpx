//
// AuthenticationSchemes.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include <common/AuthenticationSchemes.h>

#include <gtest/gtest.h>

namespace UnitTests
{
    using namespace winpx;

    TEST(AuthenticationSchemesTest, UsesCorrectDefaults)
    {
        AuthenticationSchemes schemes = AuthenticationSchemes::Defaults();
        ASSERT_FALSE(schemes.Contains(AuthenticationScheme::Basic));
        ASSERT_FALSE(schemes.Contains(AuthenticationScheme::Digest));
        ASSERT_TRUE(schemes.Contains(AuthenticationScheme::Ntlm));
        ASSERT_TRUE(schemes.Contains(AuthenticationScheme::Negotiate));
    };

    TEST(AuthenticationSchemesTest, NewCollectionIsEmpty)
    {
        AuthenticationSchemes schemes;
        ASSERT_FALSE(schemes.Contains(AuthenticationScheme::Basic));
        ASSERT_FALSE(schemes.Contains(AuthenticationScheme::Digest));
        ASSERT_FALSE(schemes.Contains(AuthenticationScheme::Ntlm));
        ASSERT_FALSE(schemes.Contains(AuthenticationScheme::Negotiate));
        ASSERT_EQ(schemes.ToValue(), 0u);
    };

    TEST(AuthenticationSchemesTest, AddSchemes)
    {
        AuthenticationSchemes schemes;
        schemes.Add(AuthenticationScheme::Basic);
        schemes.Add(AuthenticationScheme::Ntlm);
        ASSERT_TRUE(schemes.Contains(AuthenticationScheme::Basic));
        ASSERT_FALSE(schemes.Contains(AuthenticationScheme::Digest));
        ASSERT_TRUE(schemes.Contains(AuthenticationScheme::Ntlm));
        ASSERT_FALSE(schemes.Contains(AuthenticationScheme::Negotiate));
    };

    TEST(AuthenticationSchemesTest, MaskSchemes)
    {
        ASSERT_EQ(AuthenticationSchemes({ AuthenticationScheme::Basic }).Mask({ AuthenticationScheme::None }), AuthenticationSchemes({}));
        ASSERT_EQ(AuthenticationSchemes({ AuthenticationScheme::Basic }).Mask({ AuthenticationScheme::Negotiate }), AuthenticationSchemes({}));
        ASSERT_EQ(AuthenticationSchemes({ AuthenticationScheme::Basic, AuthenticationScheme::Negotiate }).Mask({ AuthenticationScheme::Basic }), AuthenticationSchemes({ AuthenticationScheme::Basic }));
        ASSERT_EQ(AuthenticationSchemes({ AuthenticationScheme::Basic, AuthenticationScheme::Negotiate }).Mask({ AuthenticationScheme::Negotiate }), AuthenticationSchemes({ AuthenticationScheme::Negotiate }));
        ASSERT_EQ(AuthenticationSchemes({ AuthenticationScheme::Basic, AuthenticationScheme::Negotiate }).Mask({ AuthenticationScheme::Any }), AuthenticationSchemes({ AuthenticationScheme::Basic, AuthenticationScheme::Negotiate }));
    };

    TEST(AuthenticationSchemesTest, PickBest)
    {
        ASSERT_EQ(AuthenticationSchemes({ AuthenticationScheme::Any }).PickBest(), AuthenticationScheme::Negotiate);
        ASSERT_EQ(AuthenticationSchemes({ AuthenticationScheme::Negotiate, AuthenticationScheme::Ntlm }).PickBest(), AuthenticationScheme::Negotiate);
        ASSERT_EQ(AuthenticationSchemes({ AuthenticationScheme::Ntlm, AuthenticationScheme::Digest }).PickBest(), AuthenticationScheme::Ntlm);
        ASSERT_EQ(AuthenticationSchemes({ AuthenticationScheme::Digest, AuthenticationScheme::Basic }).PickBest(), AuthenticationScheme::Digest);
        ASSERT_EQ(AuthenticationSchemes({ AuthenticationScheme::Basic }).PickBest(), AuthenticationScheme::Basic);
        ASSERT_EQ(AuthenticationSchemes({ AuthenticationScheme::None }).PickBest(), AuthenticationScheme::None);
    };
}
