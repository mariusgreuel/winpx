//
// HttpConfiguration.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include <chrono>

namespace winpx
{
    class HttpConfiguration
    {
    public:
        static constexpr size_t MB = 0x100000;

        size_t maxHeadSize = 1 * MB;
        size_t maxChunkSize = 64 * MB;
        size_t maxBodySizeBeforeProbing = 1 * MB;
        size_t maxReplayBufferSize = 256 * MB;
        size_t maxDrainSize = 1 * MB;

        std::chrono::milliseconds resolveTimeout = std::chrono::seconds(5);
        std::chrono::milliseconds connectTimeout = std::chrono::seconds(5);
        std::chrono::milliseconds readTimeout = std::chrono::seconds(120);
        std::chrono::milliseconds writeTimeout = std::chrono::seconds(15);
        std::chrono::milliseconds expectContinueTimeout = std::chrono::seconds(3);
        std::chrono::milliseconds drainTimeout = std::chrono::seconds(2);
    };
}
