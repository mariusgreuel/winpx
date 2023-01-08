//
// Uri.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include <cstdint>
#include <string>
#include <string_view>

namespace winpx
{
    class Uri
    {
    public:
        Uri() = default;
        Uri(std::string uri);
        Uri(std::string_view uri);
        Uri(const char* uri);
        Uri(const Uri& other);
        Uri(Uri&& other);
        Uri& operator=(Uri&& other);

        Uri& operator=(const Uri& other);

        bool IsEmpty() const;
        std::string ToString() const;

        bool IsAbsoluteUri() const;

        std::string GetOriginal() const;
        std::string GetScheme() const;
        std::string GetUsernameAndPassword() const;
        std::string GetHostAndPort() const;
        std::string GetPathAndQuery() const;
        std::string GetUsername() const;
        std::string GetPassword() const;
        std::string GetHost() const;
        uint16_t GetPort() const;
        std::string GetPath() const;
        std::string GetQuery() const;

        void Parse(std::string uri);

        std::string_view scheme;
        std::string_view usernameAndPassword;
        std::string_view hostAndPort;
        std::string_view pathAndQuery;
        std::string_view username;
        std::string_view password;
        std::string_view host;
        std::string_view path;
        std::string_view query;
        uint16_t port = 0;

    private:
        std::string original;
    };
}
