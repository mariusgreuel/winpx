//
// HttpHelper.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include "pch.h"

#include "HttpHelper.h"

#include "HttpError.h"
#include "StringHelper.h"

namespace winpx
{
    using namespace std::literals::string_view_literals;

    Uri GetAbsoluteUrl(const HttpRequestMessage& request)
    {
        Uri target(request.GetRequestUri());
        if (target.IsAbsoluteUri())
        {
            return target;
        }
        else
        {
            auto pathAndQuery = target.GetPathAndQuery();
            if (pathAndQuery == "*")
            {
                pathAndQuery = "";
            }

            if (auto host = request.GetHeaders().GetValue("Host"sv))
            {
                return Uri(std::format("http://{}{}", *host, pathAndQuery));
            }
            else
            {
                throw HttpError(HttpStatusCode::BadRequest, "Missing Host header.");
            }
        }
    }

    bool IsLocalhost(std::string_view host)
    {
        return IEquals(host, "localhost"sv) ||
               IEquals(host, "127.0.0.1"sv) ||
               IEquals(host, "::1"sv);
    }

    // 'list' can have the following format:
    // - "proxy1:8080" (global)
    // - "proxy1:8080;proxy2:8080;..." (failover)
    // - "http=proxy1:8080;https=proxy2:8080" (scheme-specific)
    std::string PickProxyFromList(const std::string& list, std::string_view url)
    {
        auto entries = SplitString(list, ';');
        if (entries.size() > 0)
        {
            auto colon = url.find(':');
            if (colon != std::string_view::npos)
            {
                auto scheme = url.substr(0, colon);
                for (const auto& entry : entries)
                {
                    auto equal = entry.find('=');
                    if (equal != std::string::npos)
                    {
                        if (IEquals(scheme, Trim(entry.substr(0, equal))))
                        {
                            return std::string{ Trim(entry.substr(equal + 1)) };
                        }
                    }
                }
            }

            const auto& entry = entries[0];

            auto equal = entry.find('=');
            if (equal != std::string::npos)
            {
                return std::string{ Trim(entry.substr(equal + 1)) };
            }
            else
            {
                return std::string{ Trim(entry) };
            }
        }
        else
        {
            return {};
        }
    }

    std::string NormalizeCrLf(std::string_view message)
    {
        if (message.find_first_of('\n') == 0)
        {
            message = message.substr(1);
        }

        std::string result(message);

        size_t pos = 0;
        while ((pos = result.find('\n', pos)) != std::string::npos)
        {
            if (pos == 0 || result[pos - 1] != '\r')
            {
                result.insert(pos, "\r");
                pos += 2;
            }
            else
            {
                pos += 1;
            }
        }

        return result;
    }

}
