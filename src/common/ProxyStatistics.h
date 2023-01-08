//
// ProxyStatistics.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once

namespace winpx
{
    struct ProxyStatistics
    {
        std::string state;
        std::string duration;
        std::atomic_size_t errors;
        std::atomic_size_t activeConnections;
        std::atomic_size_t totalConnections;
        std::atomic_size_t totalClientBytesSent;
        std::atomic_size_t totalClientBytesReceived;
        std::atomic_size_t totalUpstreamBytesSent;
        std::atomic_size_t totalUpstreamBytesReceived;
    };
}
