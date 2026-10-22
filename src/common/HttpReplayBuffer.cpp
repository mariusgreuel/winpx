//
// HttpReplayBuffer.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include "pch.h"

#include "HttpReplayBuffer.h"

#include "HttpReplayBuffer.tmh"

namespace winpx
{
    void HttpReplayBuffer::Append(std::string_view data, size_t maxBufferSize)
    {
        if (m_overflow)
        {
            return;
        }
        else if (data.size() + m_data.size() <= maxBufferSize)
        {
            m_data += data;
        }
        else
        {
            DoTraceMessage(WppWarning, "Replay buffer overflow after %Id bytes.", maxBufferSize);
            m_overflow = true;
            std::string().swap(m_data);
        }
    }

    std::string_view HttpReplayBuffer::GetData() const
    {
        return m_data;
    }

    bool HttpReplayBuffer::IsOverflow() const
    {
        return m_overflow;
    }
}
