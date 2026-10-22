//
// Logger.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include <pch.h>

#include "Logger.h"
#include "Logger.tmh"

namespace win32
{
    Logger& Logger::GetInstance()
    {
        static Logger instance;
        return instance;
    }

    Logger::Logger()
    {
    }

    Logger::~Logger()
    {
        Stop();
    }

    bool Logger::IsEnabled() const
    {
        return m_enabled;
    }

    void Logger::Start(Callback callback)
    {
        DoTraceMessage(WppVerbose, "%!FUNC!");

        Stop();

        if (!callback)
        {
            return;
        }

        m_callback = std::move(callback);
        {
            std::lock_guard lock(m_mutex);
            m_stopRequested = false;
        }

        m_thread = std::thread(&Logger::WorkerThread, this);
        m_enabled = true;
    }

    void Logger::Stop()
    {
        DoTraceMessage(WppVerbose, "%!FUNC!");

        m_enabled = false;

        if (m_thread.joinable())
        {
            {
                std::lock_guard lock(m_mutex);
                m_stopRequested = true;
            }

            m_conditionVariable.notify_one();
            m_thread.join();
        }
    }

    void Logger::LogMessage(LogLevel level, std::string_view message)
    {
        DoTraceMessage(WppVerbose, "%!FUNC!: level=%!LogLevel!, message=%!sv!", static_cast<int>(level), message);

        if (m_enabled)
        {
            while (message.ends_with('\n') || message.ends_with('\r'))
            {
                message.remove_suffix(1);
            }

            {
                std::lock_guard lock(m_mutex);
                m_entries.emplace_back(
                    LogEntry{
                        std::chrono::system_clock::now(),
                        GetCurrentProcessId(),
                        GetCurrentThreadId(),
                        level,
                        std::string(message) });
            }

            m_conditionVariable.notify_one();
        }
    }

    void Logger::WorkerThread()
    {
        bool stop = false;
        while (!stop)
        {
            std::list<LogEntry> entries;

            {
                std::unique_lock lock(m_mutex);
                m_conditionVariable.wait(lock, [this] { return m_stopRequested || !m_entries.empty(); });
                entries.swap(m_entries);
                stop = m_stopRequested;
            }

            for (const auto& entry : entries)
            {
                m_callback(entry);
            }
        }
    }
}
