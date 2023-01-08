//
// ConsoleColor.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include <pch.h>

#include "ConsoleColor.h"

namespace win32
{
    ConsoleColor::ConsoleColor(ForegroundColor color)
    {
        SaveCurrentAttributes();
        SetForegroundColor(color);
    }

    ConsoleColor::~ConsoleColor()
    {
        RestoreOldAttributes();
    }

    void ConsoleColor::SaveCurrentAttributes()
    {
        HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hStdOut != INVALID_HANDLE_VALUE)
        {
            CONSOLE_SCREEN_BUFFER_INFO csbi{};
            if (GetConsoleScreenBufferInfo(hStdOut, &csbi))
            {
                m_hStdOut = hStdOut;
                m_wOldAttributes = csbi.wAttributes;
            }
        }
    }

    void ConsoleColor::RestoreOldAttributes()
    {
        if (m_hStdOut != INVALID_HANDLE_VALUE)
        {
            SetConsoleTextAttribute(m_hStdOut, m_wOldAttributes);
        }
    }

    void ConsoleColor::SetForegroundColor(ForegroundColor color)
    {
        if (m_hStdOut != INVALID_HANDLE_VALUE)
        {
            WORD wAttributes = m_wOldAttributes & ~0xF;
            switch (color)
            {
            case ForegroundColor::Green:
                wAttributes |= FOREGROUND_INTENSITY | FOREGROUND_GREEN;
                break;
            case ForegroundColor::Yellow:
                wAttributes |= FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN;
                break;
            case ForegroundColor::Red:
                wAttributes |= FOREGROUND_INTENSITY | FOREGROUND_RED;
                break;
            case ForegroundColor::Cyan:
                wAttributes |= FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_BLUE;
                break;
            case ForegroundColor::White:
                wAttributes |= FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
                break;
            default:
                throw std::runtime_error("Invalid console color.");
            }

            SetConsoleTextAttribute(m_hStdOut, wAttributes);
        }
    }
}
