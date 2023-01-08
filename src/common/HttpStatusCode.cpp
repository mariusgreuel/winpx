//
// HttpStatusCode.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include "pch.h"

#include "HttpStatusCode.h"

namespace winpx
{
    const char* ToReasonPhrase(HttpStatusCode statusCode)
    {
        switch (statusCode)
        {
        case HttpStatusCode::Continue:
            return "Continue";
        case HttpStatusCode::SwitchingProtocols:
            return "Switching Protocols";
        case HttpStatusCode::OK:
            return "OK";
        case HttpStatusCode::Created:
            return "Created";
        case HttpStatusCode::Accepted:
            return "Accepted";
        case HttpStatusCode::NonAuthoritativeInformation:
            return "Non-Authoritative Information";
        case HttpStatusCode::NoContent:
            return "No Content";
        case HttpStatusCode::ResetContent:
            return "Reset Content";
        case HttpStatusCode::PartialContent:
            return "Partial Content";
        case HttpStatusCode::MultipleChoices:
            return "Multiple Choices";
        case HttpStatusCode::MovedPermanently:
            return "Moved Permanently";
        case HttpStatusCode::MovedTemporarily:
            return "Moved Temporarily";
        case HttpStatusCode::SeeOther:
            return "See Other";
        case HttpStatusCode::NotModified:
            return "Not Modified";
        case HttpStatusCode::UseProxy:
            return "Use Proxy";
        case HttpStatusCode::BadRequest:
            return "Bad Request";
        case HttpStatusCode::Unauthorized:
            return "Unauthorized";
        case HttpStatusCode::Forbidden:
            return "Forbidden";
        case HttpStatusCode::NotFound:
            return "Not Found";
        case HttpStatusCode::MethodNotAllowed:
            return "Method Not Allowed";
        case HttpStatusCode::NotAcceptable:
            return "Not Acceptable";
        case HttpStatusCode::ProxyAuthenticationRequired:
            return "Proxy Authentication Required";
        case HttpStatusCode::RequestTimeout:
            return "Request Timeout";
        case HttpStatusCode::Conflict:
            return "Conflict";
        case HttpStatusCode::Gone:
            return "Gone";
        case HttpStatusCode::InternalServerError:
            return "Internal Server Error";
        case HttpStatusCode::NotImplemented:
            return "Not Implemented";
        case HttpStatusCode::BadGateway:
            return "Bad Gateway";
        case HttpStatusCode::ServiceUnavailable:
            return "Service Unavailable";
        case HttpStatusCode::GatewayTimeout:
            return "Gateway Timeout";
        case HttpStatusCode::HttpVersionNotSupported:
            return "HTTP Version Not Supported";
        default:
            return "Unknown";
        }
    }
}
