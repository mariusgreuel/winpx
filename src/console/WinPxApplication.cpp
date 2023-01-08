//
// WinPxApplication.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include "pch.h"

#include "WinPxApplication.h"
#include "WinPxApplication.tmh"

#include "WinPxServiceInstaller.h"
#include <common/Proxy.h>
#include <common/ProxyCommandLineParser.h>
#include <common/Resources.h>
#include <common/Version.h>
#include <win32/ConsoleColor.h>
#include <win32/Win32Error.h>

namespace winpx
{
    using namespace std::literals::string_view_literals;
    using namespace win32;

    WinPxApplication::WinPxApplication()
    {
        DoTraceMessage(WppObject, "%!FUNC!");
    }

    WinPxApplication::~WinPxApplication()
    {
        DoTraceMessage(WppObject, "%!FUNC!");
    }

    int WinPxApplication::Run(int argc, wchar_t** argv)
    {
        PrintBanner();

        if (argc <= 1)
        {
            PrintHelp();
            return 1;
        }

        ParseCommandLineArguments(argc, argv);

        if (m_options.showHelp)
        {
            PrintHelp();
        }
        else if (m_options.showLicenses)
        {
            PrintLicenses();
        }
        else if (m_options.installService)
        {
            WinPxServiceInstaller installer(m_options.serviceName);
            installer.displayName = m_options.displayName;
            installer.serviceArguments = CollectServiceArguments(m_proxyConfiguration);
            HRESULT hr = installer.Install();
            if (FAILED(hr))
            {
                throw Win32Error(hr, std::format("Failed to install service '{}'", m_options.serviceName));
            }

            hr = installer.SetEventLogMessageFile();
            if (FAILED(hr))
            {
                throw Win32Error(hr, std::format("Failed to set event log message file for service '{}'", m_options.serviceName));
            }

            ConsoleColor color(ConsoleColor::ForegroundColor::Green);
            std::cout << std::format("Service '{}' installed successfully!", m_options.serviceName) << std::endl;

            if (m_options.run)
            {
                hr = installer.Start();
                if (FAILED(hr))
                {
                    throw Win32Error(hr, std::format("Failed to start service '{}'", m_options.serviceName));
                }

                std::cout << std::format("Service '{}' started successfully!", m_options.serviceName) << std::endl;
            }
        }
        else if (m_options.uninstallService)
        {
            WinPxServiceInstaller installer(m_options.serviceName);
            HRESULT hr = installer.Uninstall();
            if (FAILED(hr))
            {
                throw Win32Error(hr, std::format("Failed to uninstall service '{}'", m_options.serviceName));
            }

            ConsoleColor color(ConsoleColor::ForegroundColor::Green);
            std::cout << std::format("Service '{}' uninstalled successfully!", m_options.serviceName) << std::endl;
        }
        else if (m_options.run)
        {
            std::cout << "Starting WinPX proxy server..." << std::endl;

            Proxy proxy(m_proxyConfiguration);
            proxy.Start();
            if (proxy.GetState() == ProxyState::Error)
                throw std::system_error(proxy.GetErrorCode());

            std::cout << "WinPX proxy serving requests via " << m_proxyConfiguration.GetWinPxProxyUrl() << std::endl;
            std::cout << "Press CTRL-C to exit." << std::endl;

            proxy.WaitForIoShutdown();
            proxy.Stop();
        }

        return 0;
    }

    void WinPxApplication::ParseCommandLineArguments(int argc, wchar_t** argv)
    {
        ProxyCommandLineParser parser(m_proxyConfiguration);
        parser.AddOption("run"sv, m_options.run);
        parser.AddOption("install-service"sv, m_options.installService);
        parser.AddOption("uninstall-service"sv, m_options.uninstallService);
        parser.AddOption("service-name"sv, m_options.serviceName);
        parser.AddOption("display-name"sv, m_options.displayName);
        parser.AddOption("licenses"sv, m_options.showLicenses);
        parser.AddOption("help"sv, m_options.showHelp);
        parser.AddAlias("r"sv, "run"sv);
        parser.AddAlias("h"sv, "help"sv);
        parser.AddAlias("?"sv, "help"sv);
        parser.Parse(argc, argv);
    }

    std::string WinPxApplication::CollectServiceArguments(const ProxyConfiguration& proxyConfiguration)
    {
        std::string arguments;
        auto AddArgument = [&](const std::string& argument) {
            if (!arguments.empty())
            {
                arguments += ' ';
            }

            arguments += argument;
        };
        auto MapProxyMode = [](ProxyMode proxyMode) -> std::string {
            switch (proxyMode)
            {
            case ProxyMode::Default:
            case ProxyMode::System:
                return "system";
            case ProxyMode::Direct:
                return "direct";
            case ProxyMode::Manual:
                return "manual";
            case ProxyMode::AutoConfig:
                return "pac";
            case ProxyMode::AutoDetect:
                return "wpad";
            default:
                throw std::invalid_argument("Invalid proxy mode.");
            }
        };

        if (proxyConfiguration.port != 3128)
            AddArgument("--port=" + std::to_string(proxyConfiguration.port));
        if (proxyConfiguration.proxyMode != ProxyMode::Default)
            AddArgument("--mode=" + MapProxyMode(proxyConfiguration.proxyMode));
        if (!proxyConfiguration.autoConfigUrl.empty())
            AddArgument("--pac-url=" + proxyConfiguration.autoConfigUrl);
        if (!proxyConfiguration.httpProxyUrl.empty())
            AddArgument("--http-proxy=" + proxyConfiguration.httpProxyUrl);
        if (!proxyConfiguration.httpsProxyUrl.empty())
            AddArgument("--https-proxy=" + proxyConfiguration.httpsProxyUrl);
        if (proxyConfiguration.allowedAuthenticationSchemes != AuthenticationSchemes::Defaults())
        {
            if (proxyConfiguration.allowedAuthenticationSchemes == AuthenticationSchemes{ AuthenticationScheme::Any })
            {
                AddArgument("--auth=any");
            }
            else if (proxyConfiguration.allowedAuthenticationSchemes == AuthenticationSchemes{})
            {
                AddArgument("--auth=none");
            }
            else
            {
                if (proxyConfiguration.allowedAuthenticationSchemes.Contains(AuthenticationScheme::Negotiate))
                    AddArgument("--auth=negotiate");
                if (proxyConfiguration.allowedAuthenticationSchemes.Contains(AuthenticationScheme::Ntlm))
                    AddArgument("--auth=ntlm");
                if (proxyConfiguration.allowedAuthenticationSchemes.Contains(AuthenticationScheme::Digest))
                    AddArgument("--auth=digest");
                if (proxyConfiguration.allowedAuthenticationSchemes.Contains(AuthenticationScheme::Basic))
                    AddArgument("--auth=basic");
            }
        }
        if (!proxyConfiguration.gatewayUsername.empty())
            AddArgument("--username=" + proxyConfiguration.gatewayUsername);
        if (!proxyConfiguration.gatewayPassword.empty())
            AddArgument("--password=" + proxyConfiguration.gatewayPassword);
        if (!proxyConfiguration.winpxSecret.empty())
            AddArgument("--secret=" + proxyConfiguration.winpxSecret);
        if (proxyConfiguration.setEnvironmentVariables)
            AddArgument("--environment");

        return arguments;
    }

    void WinPxApplication::PrintBanner()
    {
        std::string gitCommitHash = GIT_COMMIT_HASH;
        std::cout << "WinPX V" << VERSION_MAJOR << "." << VERSION_MINOR << "." << VERSION_PATCH << " (" << gitCommitHash.substr(0, 8) << ")" << std::endl;
        std::cout << "Copyright (c) 2021 Marius Greuel. All rights reserved." << std::endl;
    }

    void WinPxApplication::PrintHelp()
    {
        std::cout << "Usage: winpxc [OPTION]..." << std::endl;
        std::cout << "Starts a local proxy server that connects to an enterprise proxy server." << std::endl;
        std::cout << std::endl;
        std::cout << "Options:" << std::endl;
        std::cout << "  --run, -r                    start WinPX proxy server" << std::endl;
        std::cout << "  --port=PORT, -p=PORT         set the PORT that WinPX listens on" << std::endl;
        std::cout << "  --mode=MODE, -m=MODE         select the proxy MODE: system, pac, wpad, manual" << std::endl;
        std::cout << "  --pac-url=URL                set the proxy auto-config URL" << std::endl;
        std::cout << "  --http-proxy=HOST:PORT       set the upstream proxy HOST and PORT used for HTTP requests" << std::endl;
        std::cout << "  --https-proxy=HOST:PORT      set the upstream proxy HOST and PORT used for HTTPS requests" << std::endl;
        std::cout << "  --auth=SCHEME                set the upstream proxy authentication SCHEME: negotiate, ntlm, digest, basic, any, none" << std::endl;
        std::cout << "  --username=USERNAME          set the upstream proxy USERNAME for digest authentication" << std::endl;
        std::cout << "  --password=PASSWORD          set the upstream proxy PASSWORD for digest authentication" << std::endl;
        std::cout << "  --secret=SECRET              set the SECRET for local authentication" << std::endl;
        std::cout << "  --environment, -e            set http_proxy/https_proxy environment variables to use WinPX" << std::endl;
        std::cout << "  --install-service            install WinPX as a Windows service" << std::endl;
        std::cout << "  --uninstall-service          uninstall WinPX service" << std::endl;
        std::cout << "  --service-name=NAME          set the name of the Windows service" << std::endl;
        std::cout << "  --display-name=NAME          set the display name of the Windows service" << std::endl;
        std::cout << "  --licenses                   show WinPX license and license attributions" << std::endl;
        std::cout << "  --help, -h                   display this help message" << std::endl;
        std::cout << std::endl;
        std::cout << "Example:" << std::endl;
        std::cout << "  winpxc --run" << std::endl;
        std::cout << std::endl;
    }

    void WinPxApplication::PrintLicenses()
    {
        std::cout << std::endl;
        std::cout << Resources::GetLicenses();
    }
}
