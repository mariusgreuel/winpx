//
// HttpError.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include "HttpStatusCode.h"

#include <exception>
#include <string>

namespace winpx
{
    class HttpError : public std::exception
    {
    public:
        explicit HttpError(HttpStatusCode statusCode) :
            std::exception(ToReasonPhrase(statusCode)),
            m_statusCode(statusCode),
            m_reasonPhrase(ToReasonPhrase(statusCode))
        {
        }

        explicit HttpError(HttpStatusCode statusCode, const char* reasonPhrase) :
            std::exception(reasonPhrase),
            m_statusCode(statusCode),
            m_reasonPhrase(reasonPhrase)
        {
        }

        explicit HttpError(const char* message, HttpStatusCode statusCode) :
            std::exception(message),
            m_statusCode(statusCode),
            m_reasonPhrase(ToReasonPhrase(statusCode))
        {
        }

        explicit HttpError(const char* message, HttpStatusCode statusCode, const char* reasonPhrase) :
            std::exception(message),
            m_statusCode(statusCode),
            m_reasonPhrase(reasonPhrase)
        {
        }

        HttpStatusCode GetStatusCode() const { return m_statusCode; }
        std::string GetReasonPhrase() const { return m_reasonPhrase; }

    private:
        HttpStatusCode m_statusCode = HttpStatusCode::Unknown;
        std::string m_reasonPhrase;
    };
}
