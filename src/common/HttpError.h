//
// HttpError.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include "HttpStatusCode.h"

namespace winpx
{
    class HttpError : public std::exception
    {
    public:
        explicit HttpError(HttpStatusCode statusCode) :
            std::exception(ToReasonPhrase(statusCode)),
            m_statusCode(statusCode)
        {
        }

        explicit HttpError(HttpStatusCode statusCode, const char* reasonPhrase) :
            std::exception(reasonPhrase),
            m_statusCode(statusCode)
        {
        }

        HttpStatusCode GetStatusCode() const { return m_statusCode; }
        std::string GetReasonPhrase() const { return what(); }

    private:
        HttpStatusCode m_statusCode = HttpStatusCode::Unknown;
    };
}
