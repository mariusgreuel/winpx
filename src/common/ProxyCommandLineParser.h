//
// ProxyCommandLineParser.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include "CommandLineParser.h"
#include "ProxyConfiguration.h"

namespace winpx
{
    class ProxyCommandLineParser : public CommandLineParser
    {
    public:
        ProxyCommandLineParser(ProxyConfiguration& configuration);

    private:
        AuthenticationSchemes m_allowedAuthenticationSchemes;
    };
}
