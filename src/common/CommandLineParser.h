//
// CommandLineParser.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace winpx
{
    class CommandLineParser
    {
    public:
        using Callback = std::function<void(std::string_view value)>;

        enum class Type
        {
            Unknown,
            Bool,
            String,
            Callback,
        };

        struct Option
        {
            Option(std::string_view name) : name(name)
            {}

            virtual ~Option() = default;
            Option(const Option&) = delete;
            Option& operator=(const Option&) = delete;

            std::string name;
            virtual Type GetType() const = 0;
        };

        struct BoolOption : public Option
        {
            BoolOption(std::string_view name, bool& value) : Option(name), value(value)
            {}

            bool& value;
            Type GetType() const override { return Type::Bool; }
        };

        struct StringOption : public Option
        {
            StringOption(std::string_view name, std::string& value) : Option(name), value(value)
            {}

            std::string& value;
            Type GetType() const override { return Type::String; }
        };

        struct CallbackOption : public Option
        {
            CallbackOption(std::string_view name, Callback callback) : Option(name), callback(std::move(callback))
            {}

            const Callback callback;
            Type GetType() const override { return Type::Callback; }
        };

        void AddOption(std::string_view name, bool& value);
        void AddOption(std::string_view name, std::string& value);
        void AddOption(std::string_view name, const Callback callback);
        void AddAlias(std::string_view alias, std::string_view name);

        void Parse();
        void Parse(int argc, char** argv);
        void Parse(int argc, wchar_t** argv);
        void Parse(const std::vector<std::string>& arguments);

    private:
        void ParseOption(const std::string& argument);
        const Option& GetOption(const std::string& name);
        static void SetValue(const Option& option);
        static void SetValue(const Option& option, const std::string& value);

        std::unordered_map<std::string, std::unique_ptr<Option>> m_options;
        std::unordered_map<std::string, std::string> m_aliases;
    };
}
