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

        bool setTcpNoDelay = true;

        size_t maxHeadSize = 1 * MB;
        size_t maxChunkSize = 64 * MB;
        size_t maxBodySizeBeforeProbing = 1 * MB;
        size_t maxReplayBufferSize = 256 * MB;
        size_t maxDrainSize = 1 * MB;
        size_t maxIdleServerConnectionsPerHost = 8;

        std::chrono::milliseconds resolveTimeout = std::chrono::milliseconds(5000);
        std::chrono::milliseconds connectTimeout = std::chrono::milliseconds(5000);
        std::chrono::milliseconds connectionAttemptDelay = std::chrono::milliseconds(250);
        std::chrono::milliseconds keepAliveTimeout = std::chrono::milliseconds(30000);
        std::chrono::milliseconds serverIdleTimeout = std::chrono::milliseconds(30000);
        std::chrono::milliseconds tunnelTimeout = std::chrono::milliseconds(900000);
        std::chrono::milliseconds readTimeout = std::chrono::milliseconds(120000);
        std::chrono::milliseconds writeTimeout = std::chrono::milliseconds(15000);
        std::chrono::milliseconds expectContinueTimeout = std::chrono::milliseconds(1000);
        std::chrono::milliseconds drainTimeout = std::chrono::milliseconds(2000);
    };
}
