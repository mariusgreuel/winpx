//
// AuthenticationSchemes.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include <cstdint>
#include <initializer_list>
#include <stdexcept>

namespace winpx
{
    enum class AuthenticationScheme
    {
        None = 0,
        Basic = 1,
        Digest = 2,
        Ntlm = 3,
        Negotiate = 4,
        Any = 5,
    };

    class AuthenticationSchemes
    {
    public:
        AuthenticationSchemes() = default;

        AuthenticationSchemes(uint32_t schemes) : m_schemes(schemes)
        {
        }

        AuthenticationSchemes(std::initializer_list<AuthenticationScheme> schemes) : m_schemes(ToBits(schemes))
        {
        }

        static AuthenticationSchemes Defaults()
        {
            return { AuthenticationScheme::Negotiate, AuthenticationScheme::Ntlm };
        }

        bool Contains(AuthenticationScheme authenticationScheme) const
        {
            return (m_schemes & ToBit(authenticationScheme)) != 0 || (m_schemes & ToBit(AuthenticationScheme::Any)) != 0;
        }

        uint32_t ToValue() const
        {
            return m_schemes;
        }

        void Add(AuthenticationScheme authenticationScheme)
        {
            m_schemes |= ToBit(authenticationScheme);
        }

        void Clear()
        {
            m_schemes = 0;
        }

        AuthenticationSchemes Mask(AuthenticationSchemes allowedSchemes) const
        {
            if (allowedSchemes.Contains(AuthenticationScheme::Any))
            {
                return *this;
            }
            else
            {
                return AuthenticationSchemes(m_schemes & allowedSchemes.m_schemes);
            }
        }

        AuthenticationScheme PickBest() const
        {
            if (Contains(AuthenticationScheme::Negotiate))
                return AuthenticationScheme::Negotiate;

            if (Contains(AuthenticationScheme::Ntlm))
                return AuthenticationScheme::Ntlm;

            if (Contains(AuthenticationScheme::Digest))
                return AuthenticationScheme::Digest;

            if (Contains(AuthenticationScheme::Basic))
                return AuthenticationScheme::Basic;

            return AuthenticationScheme::None;
        }

    private:
        static uint32_t ToBit(AuthenticationScheme scheme)
        {
            return 1u << static_cast<uint32_t>(scheme);
        }

        static uint32_t ToBits(std::initializer_list<AuthenticationScheme> schemes)
        {
            uint32_t bits = 0;
            for (auto scheme : schemes)
            {
                bits |= ToBit(scheme);
            }
            return bits;
        }

        uint32_t m_schemes = 0;
    };

    inline bool operator==(const AuthenticationSchemes& lhs, const AuthenticationSchemes& rhs)
    {
        return lhs.ToValue() == rhs.ToValue();
    }

    inline std::string ToString(AuthenticationScheme scheme)
    {
        switch (scheme)
        {
        case AuthenticationScheme::Basic:
            return "Basic";
        case AuthenticationScheme::Digest:
            return "Digest";
        case AuthenticationScheme::Ntlm:
            return "NTLM";
        case AuthenticationScheme::Negotiate:
            return "Negotiate";
        default:
            throw std::invalid_argument("Invalid authentication scheme.");
        }
    }
}
