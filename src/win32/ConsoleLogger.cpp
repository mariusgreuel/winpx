//
// ConsoleLogger.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include <pch.h>

#include "ConsoleColor.h"
#include "ConsoleLogger.h"

#include <iostream>

namespace win32
{
    bool ConsoleLogger::IsEnabled()
    {
        return Logger::GetInstance().IsEnabled();
    }

    void ConsoleLogger::Start()
    {
        Logger::GetInstance().Start([](const LogEntry& entry) {
            auto timestamp = std::chrono::round<std::chrono::milliseconds>(entry.timestamp);
            auto localTime = std::chrono::zoned_time(std::chrono::current_zone(), timestamp).get_local_time();
            std::cout << localTime << " ";

            switch (entry.level)
            {
            case LogLevel::Info:
            {
                std::cout << "[INFO] ";
                break;
            }
            case LogLevel::Warning:
            {
                ConsoleColor color(ConsoleColor::ForegroundColor::Yellow);
                std::cout << "[WARNING] ";
                break;
            }
            case LogLevel::Error:
            {
                ConsoleColor color(ConsoleColor::ForegroundColor::Red);
                std::cout << "[ERROR] ";
                break;
            }
            }

            std::cout << entry.message << std::endl;
        });
    }

    void ConsoleLogger::Stop()
    {
        Logger::GetInstance().Stop();
    }
}
