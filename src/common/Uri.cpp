//
// Uri.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include "pch.h"

#include "Uri.h"

#include "Tools.h"

namespace winpx
{
    using namespace std::literals::string_view_literals;

    Uri::Uri(std::string uri)
    {
        Parse(std::move(uri));
    }

    Uri::Uri(std::string_view uri)
    {
        Parse(std::string(uri));
    }

    Uri::Uri(const char* uri)
    {
        Parse(std::string(uri));
    }

    Uri::Uri(const Uri& other)
    {
        Parse(other.original);
    }

    Uri::Uri(Uri&& other)
    {
        Parse(std::move(other.original));
    }

    Uri& Uri::operator=(const Uri& other)
    {
        Parse(other.original);
        return *this;
    }

    Uri& Uri::operator=(Uri&& other)
    {
        Parse(std::move(other.original));
        return *this;
    }

    bool Uri::IsEmpty() const
    {
        return original.empty();
    }

    std::string Uri::ToString() const
    {
        return original;
    }

    bool Uri::IsAbsoluteUri() const
    {
        return IStartsWith(original, "http://"sv) || IStartsWith(original, "https://"sv);
    }

    std::string Uri::GetOriginal() const
    {
        return original;
    }

    std::string Uri::GetScheme() const
    {
        return std::string(scheme);
    }

    std::string Uri::GetUsernameAndPassword() const
    {
        return std::string(usernameAndPassword);
    }

    std::string Uri::GetHostAndPort() const
    {
        bool hasColon = host.find(':') != std::string_view::npos;
        return hasColon ? std::format("[{}]:{}", host, port) : std::format("{}:{}", host, port);
    }

    std::string Uri::GetPathAndQuery() const
    {
        if (pathAndQuery.empty())
        {
            return "/";
        }
        else if (pathAndQuery.starts_with('?'))
        {
            return '/' + std::string(pathAndQuery);
        }
        else
        {
            return std::string(pathAndQuery);
        }
    }

    std::string Uri::GetUsername() const
    {
        return std::string(username);
    }

    std::string Uri::GetPassword() const
    {
        return std::string(password);
    }

    std::string Uri::GetHost() const
    {
        return std::string(host);
    }

    uint16_t Uri::GetPort() const
    {
        return port;
    }

    std::string Uri::GetPath() const
    {
        return std::string(path);
    }

    std::string Uri::GetQuery() const
    {
        return std::string(query);
    }

    // sheme://[username]:[password]@[host]:[port][/some/path[?query]]
    void Uri::Parse(std::string uri)
    {
        original = std::move(uri);
        std::string_view work = original;

        size_t pos = work.find("://");
        if (pos != std::string_view::npos)
        {
            scheme = work.substr(0, pos);
            work = work.substr(pos + 3);
        }
        else
        {
            scheme = {};
        }

        if ((pos = work.find('@')) != std::string_view::npos)
        {
            usernameAndPassword = work.substr(0, pos);
            work = work.substr(pos + 1);
        }
        else
        {
            usernameAndPassword = {};
        }

        if ((pos = work.find_first_of("/?#")) != std::string_view::npos)
        {
            hostAndPort = work.substr(0, pos);
            if (work[pos] != '#')
            {
                auto fragment = work.find('#', pos);
                pathAndQuery = work.substr(pos, fragment - pos);
            }
            else
            {
                pathAndQuery = {};
            }
        }
        else
        {
            hostAndPort = work;
            pathAndQuery = {};
        }

        if ((pos = usernameAndPassword.find(':')) != std::string_view::npos)
        {
            username = usernameAndPassword.substr(0, pos);
            password = usernameAndPassword.substr(pos + 1);
        }
        else
        {
            username = usernameAndPassword;
            password = {};
        }

        if (hostAndPort.starts_with('['))
        {
            auto closingBracket = hostAndPort.find(']');
            if (closingBracket == std::string_view::npos)
                throw std::invalid_argument("Invalid IPv6 address.");

            host = hostAndPort.substr(1, closingBracket - 1);
            if (closingBracket + 1 < hostAndPort.size())
            {
                if (hostAndPort[closingBracket + 1] != ':')
                    throw std::invalid_argument("Invalid URI authority.");

                port = ParseDec<uint16_t>(hostAndPort.substr(closingBracket + 2));
            }
            else if (IEquals(scheme, "http"sv))
            {
                port = 80;
            }
            else if (IEquals(scheme, "https"sv))
            {
                port = 443;
            }
            else
            {
                port = 0;
            }
        }
        else
        {
            if ((pos = hostAndPort.find(':')) != std::string_view::npos)
            {
                host = hostAndPort.substr(0, pos);
                port = ParseDec<uint16_t>(hostAndPort.substr(pos + 1));
            }
            else
            {
                host = hostAndPort;

                if (IEquals(scheme, "http"sv))
                {
                    port = 80;
                }
                else if (IEquals(scheme, "https"sv))
                {
                    port = 443;
                }
                else
                {
                    port = 0;
                }
            }
        }

        if ((pos = pathAndQuery.find('?')) != std::string_view::npos)
        {
            path = pathAndQuery.substr(0, pos);
            query = pathAndQuery.substr(pos + 1);
        }
        else
        {
            path = pathAndQuery;
            query = {};
        }
    }
}
