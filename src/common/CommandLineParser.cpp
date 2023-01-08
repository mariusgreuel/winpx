//
// CommandLineParser.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include "pch.h"

#include "CommandLineParser.h"
#include "CommandLineParser.tmh"

#include <win32/Unicode.h>

#include <format>

namespace winpx
{
    using namespace win32;

    void CommandLineParser::AddOption(std::string_view name, bool& value)
    {
        m_options.emplace(name, std::make_unique<BoolOption>(name, value));
    }

    void CommandLineParser::AddOption(std::string_view name, std::string& value)
    {
        m_options.emplace(name, std::make_unique<StringOption>(name, value));
    }

    void CommandLineParser::AddOption(std::string_view name, const Callback callback)
    {
        m_options.emplace(name, std::make_unique<CallbackOption>(name, callback));
    }

    void CommandLineParser::AddAlias(std::string_view alias, std::string_view name)
    {
        m_aliases.emplace(alias, name);
    }

    void CommandLineParser::Parse()
    {
        int nArguments = 0;
        CHeapPtr<LPWSTR, CLocalAllocator> pszArguments(CommandLineToArgvW(GetCommandLineW(), &nArguments));
        if (pszArguments)
        {
            std::vector<std::string> arguments;
            for (int i = 1; i < nArguments; i++)
            {
                arguments.push_back(Unicode::ToUtf8(pszArguments[i]));
            }

            Parse(arguments);
        }
    }

    void CommandLineParser::Parse(int argc, char** argv)
    {
        std::vector<std::string> arguments;

        for (int i = 1; i < argc; i++)
        {
            arguments.emplace_back(argv[i]);
        }

        Parse(arguments);
    }

    void CommandLineParser::Parse(int argc, wchar_t** argv)
    {
        std::vector<std::string> arguments;

        for (int i = 1; i < argc; i++)
        {
            arguments.emplace_back(Unicode::ToUtf8(argv[i]));
        }

        Parse(arguments);
    }

    void CommandLineParser::Parse(const std::vector<std::string>& arguments)
    {
        DoTraceMessage(WppVerbose, "%!FUNC!");

        for (size_t i = 0; i < arguments.size(); i++)
        {
            const auto& argument = arguments[i];
            DoTraceMessage(WppVerbose, "Parsing argument[%Iu]=%!str!", i, argument);

            if (argument.size() >= 4 && argument[0] == '-' && argument[1] == '-')
            {
                ParseOption(argument.substr(2));
            }
            else if (argument.size() >= 1 && argument[0] == '-')
            {
                ParseOption(argument.substr(1));
            }
            else if (argument.size() >= 1 && argument[0] == '/')
            {
                ParseOption(argument.substr(1));
            }
            else
            {
                throw std::runtime_error(std::format("Invalid command-line argument '{}'.", argument));
            }
        }
    }

    void CommandLineParser::ParseOption(const std::string& argument)
    {
        auto pos = argument.find('=');
        if (pos != std::string::npos)
        {
            auto name = argument.substr(0, pos);
            auto value = argument.substr(pos + 1);
            auto& option = GetOption(name);
            SetValue(option, value);
        }
        else
        {
            auto& option = GetOption(argument);
            SetValue(option);
        }
    }

    const CommandLineParser::Option& CommandLineParser::GetOption(const std::string& name)
    {
        auto alias = m_aliases.find(name);
        if (alias != m_aliases.end())
        {
            return GetOption(alias->second);
        }

        auto option = m_options.find(name);
        if (option == m_options.end())
        {
            throw std::runtime_error(std::format("Invalid command-line option '{}'.", name));
        }

        return *option->second;
    }

    void CommandLineParser::SetValue(const Option& option)
    {
        switch (option.GetType())
        {
        case Type::Bool:
            static_cast<const BoolOption&>(option).value = true;
            break;
        default:
            throw std::runtime_error(std::format("Command-line option '{}' requires an argument.", option.name));
        }
    }

    void CommandLineParser::SetValue(const Option& option, const std::string& value)
    {
        switch (option.GetType())
        {
        case Type::String:
            static_cast<const StringOption&>(option).value = value;
            break;
        case Type::Callback:
            static_cast<const CallbackOption&>(option).callback(value);
            break;
        default:
            throw std::runtime_error(std::format("Command-line option '{}' does not expect an argument.", option.name));
        }
    }
}
