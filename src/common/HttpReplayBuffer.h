//
// HttpReplayBuffer.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include <string>
#include <string_view>

namespace winpx
{
    class HttpReplayBuffer
    {
    public:
        void Append(std::string_view data, size_t maxBufferSize);
        std::string_view GetData() const;
        bool IsOverflow() const;

    private:
        std::string m_data;
        bool m_overflow = false;
    };
}
