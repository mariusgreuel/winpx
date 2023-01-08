//
// HttpStatusCode.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once

namespace winpx
{
    enum class HttpStatusCode
    {
        Unknown = 0,
        Continue = 100,
        SwitchingProtocols = 101,
        OK = 200,
        Created = 201,
        Accepted = 202,
        NonAuthoritativeInformation = 203,
        NoContent = 204,
        ResetContent = 205,
        PartialContent = 206,
        MultipleChoices = 300,
        MovedPermanently = 301,
        MovedTemporarily = 302,
        SeeOther = 303,
        NotModified = 304,
        UseProxy = 305,
        BadRequest = 400,
        Unauthorized = 401,
        Forbidden = 403,
        NotFound = 404,
        MethodNotAllowed = 405,
        NotAcceptable = 406,
        ProxyAuthenticationRequired = 407,
        RequestTimeout = 408,
        Conflict = 409,
        Gone = 410,
        InternalServerError = 500,
        NotImplemented = 501,
        BadGateway = 502,
        ServiceUnavailable = 503,
        GatewayTimeout = 504,
        HttpVersionNotSupported = 505,
    };

    const char* ToReasonPhrase(HttpStatusCode statusCode);
}
