//
// UriTests.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include <common/Uri.h>

#include <gtest/gtest.h>

namespace UnitTests
{
    using namespace winpx;

    TEST(UriTests, DefaultPort)
    {
        Uri uri("example.com");
        ASSERT_EQ(0, uri.port);
    };

    TEST(UriTests, UrlHttp)
    {
        Uri uri("http://example.com");
        ASSERT_EQ(80, uri.port);
    };

    TEST(UriTests, UrlHttps)
    {
        Uri uri("https://example.com");
        ASSERT_EQ(443, uri.port);
    };

    TEST(UriTests, UrlHttpAndPort)
    {
        Uri uri("http://example.com:1234/");
        ASSERT_EQ(1234, uri.port);
    };

    TEST(UriTests, UrlHttpsAndPort)
    {
        Uri uri("https://example.com:1234/");
        ASSERT_EQ(1234, uri.port);
    };

    TEST(UriTests, IPv6HostAndPort)
    {
        Uri uri("http://[2001:db8::1]:8080/");
        ASSERT_EQ("2001:db8::1", uri.host);
        ASSERT_EQ(8080, uri.port);
        ASSERT_EQ("[2001:db8::1]:8080", uri.GetHostAndPort());
    };

    TEST(UriTests, IPv6HostWithDefaultPort)
    {
        Uri uri("https://[::1]/");
        ASSERT_EQ("::1", uri.host);
        ASSERT_EQ(443, uri.port);
        ASSERT_EQ("[::1]:443", uri.GetHostAndPort());
    };

    TEST(UriTests, RejectsInvalidPort)
    {
        ASSERT_THROW(Uri("http://example.com:abc/"), std::invalid_argument);
        ASSERT_THROW(Uri("http://example.com:70000/"), std::out_of_range);
        ASSERT_THROW(Uri("http://[::1/"), std::invalid_argument);
    };

    TEST(UriTests, Host)
    {
        Uri uri("example.com");
        ASSERT_TRUE(uri.scheme.empty());
        ASSERT_TRUE(uri.usernameAndPassword.empty());
        ASSERT_TRUE(uri.username.empty());
        ASSERT_TRUE(uri.password.empty());
        ASSERT_EQ("example.com", uri.hostAndPort);
        ASSERT_EQ("example.com", uri.host);
        ASSERT_EQ(0, uri.port);
        ASSERT_TRUE(uri.pathAndQuery.empty());
        ASSERT_TRUE(uri.path.empty());
        ASSERT_TRUE(uri.query.empty());
    };

    TEST(UriTests, HostAndPort)
    {
        Uri uri("example.com:443");
        ASSERT_TRUE(uri.scheme.empty());
        ASSERT_TRUE(uri.usernameAndPassword.empty());
        ASSERT_TRUE(uri.username.empty());
        ASSERT_TRUE(uri.password.empty());
        ASSERT_EQ("example.com:443", uri.hostAndPort);
        ASSERT_EQ("example.com", uri.host);
        ASSERT_EQ(443, uri.port);
        ASSERT_TRUE(uri.pathAndQuery.empty());
        ASSERT_TRUE(uri.path.empty());
        ASSERT_TRUE(uri.query.empty());
    };

    TEST(UriTests, HostAndPortAndCredentials)
    {
        Uri uri("http://username:password@example.com:443");
        ASSERT_EQ("http", uri.scheme);
        ASSERT_EQ("username:password", uri.usernameAndPassword);
        ASSERT_EQ("username", uri.username);
        ASSERT_EQ("password", uri.password);
        ASSERT_EQ("example.com:443", uri.hostAndPort);
        ASSERT_EQ("example.com", uri.host);
        ASSERT_EQ(443, uri.port);
        ASSERT_TRUE(uri.pathAndQuery.empty());
        ASSERT_TRUE(uri.path.empty());
        ASSERT_TRUE(uri.query.empty());
    };

    TEST(UriTests, HostAndPortAndPath)
    {
        Uri uri("https://example.com:8080/file.html");
        ASSERT_EQ("https", uri.scheme);
        ASSERT_TRUE(uri.usernameAndPassword.empty());
        ASSERT_TRUE(uri.username.empty());
        ASSERT_TRUE(uri.password.empty());
        ASSERT_EQ("example.com:8080", uri.hostAndPort);
        ASSERT_EQ("example.com", uri.host);
        ASSERT_EQ(8080, uri.port);
        ASSERT_EQ("/file.html", uri.pathAndQuery);
        ASSERT_EQ("/file.html", uri.path);
        ASSERT_TRUE(uri.query.empty());
    };

    TEST(UriTests, FullInfo)
    {
        Uri uri("https://username:password@example.com:443/some/path/file.html?key1=value1&key2=value2");
        ASSERT_EQ("https", uri.scheme);
        ASSERT_EQ("username:password", uri.usernameAndPassword);
        ASSERT_EQ("username", uri.username);
        ASSERT_EQ("password", uri.password);
        ASSERT_EQ("example.com:443", uri.hostAndPort);
        ASSERT_EQ("example.com", uri.host);
        ASSERT_EQ(443, uri.port);
        ASSERT_EQ("/some/path/file.html?key1=value1&key2=value2", uri.pathAndQuery);
        ASSERT_EQ("/some/path/file.html", uri.path);
        ASSERT_EQ("key1=value1&key2=value2", uri.query);
    };

    TEST(UriTests, ParseClearsPreviousComponents)
    {
        Uri uri("https://user:password@example.com/path?query");
        uri.Parse("example.org");

        ASSERT_TRUE(uri.scheme.empty());
        ASSERT_TRUE(uri.usernameAndPassword.empty());
        ASSERT_EQ("example.org", uri.host);
        ASSERT_TRUE(uri.pathAndQuery.empty());
        ASSERT_TRUE(uri.query.empty());
        ASSERT_EQ(0, uri.port);
    };

    TEST(UriTests, SuppliesDefaultPathAndOmitsFragment)
    {
        Uri emptyPath("http://example.com#fragment");
        ASSERT_EQ("/", emptyPath.GetPathAndQuery());

        Uri queryOnly("http://example.com?key=value#fragment");
        ASSERT_EQ("/?key=value", queryOnly.GetPathAndQuery());

        Uri path("http://example.com/path?key=value#fragment");
        ASSERT_EQ("/path?key=value", path.GetPathAndQuery());
        ASSERT_EQ("/path", path.GetPath());
        ASSERT_EQ("key=value", path.GetQuery());
    };
}
