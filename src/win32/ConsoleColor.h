//
// ConsoleColor.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once

namespace win32
{
    class ConsoleColor
    {
    public:
        enum class ForegroundColor
        {
            Green,
            Yellow,
            Red,
            Cyan,
            White,
        };

    public:
        ConsoleColor(ForegroundColor color);
        ~ConsoleColor();

        ConsoleColor(const ConsoleColor&) = delete;
        ConsoleColor& operator=(const ConsoleColor&) = delete;

    private:
        void SaveCurrentAttributes();
        void RestoreOldAttributes();
        void SetForegroundColor(ForegroundColor color);

    private:
        HANDLE m_hStdOut = INVALID_HANDLE_VALUE;
        WORD m_wOldAttributes = 0;
    };
}
