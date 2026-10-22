//
// Logger.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <format>
#include <functional>
#include <list>
#include <mutex>
#include <string_view>
#include <thread>

namespace win32
{
    enum class LogLevel
    {
        Info,
        Warning,
        Error
    };

    struct LogEntry
    {
        std::chrono::system_clock::time_point timestamp;
        uint32_t processId;
        uint32_t threadId;
        LogLevel level;
        std::string message;
    };

    class Logger
    {
    public:
        using Callback = std::function<void(const LogEntry&)>;

        static Logger& GetInstance();

        bool IsEnabled() const;

        void Start(Callback callback);
        void Stop();

        void LogMessage(LogLevel level, std::string_view message);

        template<class... Args>
        void LogMessage(LogLevel level, const std::format_string<Args...> format, Args&&... args)
        {
            LogMessage(level, std::vformat(format.get(), std::make_format_args(args...)));
        }

    private:
        Logger();
        ~Logger();
        Logger(const Logger&) = delete;
        Logger& operator=(const Logger&) = delete;

        void WorkerThread();

    private:
        std::thread m_thread;
        std::mutex m_mutex;
        std::condition_variable m_conditionVariable;
        std::list<LogEntry> m_entries;
        std::atomic<bool> m_enabled = false;
        bool m_stopRequested = false;
        Callback m_callback;
    };
}

#define LOG_INFO(...) \
    { \
        if (win32::Logger::GetInstance().IsEnabled()) \
        { \
            win32::Logger::GetInstance().LogMessage(win32::LogLevel::Info, __VA_ARGS__); \
        } \
    }
#define LOG_WARNING(...) \
    { \
        if (win32::Logger::GetInstance().IsEnabled()) \
        { \
            win32::Logger::GetInstance().LogMessage(win32::LogLevel::Warning, __VA_ARGS__); \
        } \
    }
#define LOG_ERROR(...) \
    { \
        if (win32::Logger::GetInstance().IsEnabled()) \
        { \
            win32::Logger::GetInstance().LogMessage(win32::LogLevel::Error, __VA_ARGS__); \
        } \
    }
