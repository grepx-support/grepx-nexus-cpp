/*
 * Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
 * This file is part of the GrepX platform and is proprietary and confidential.
 * Unauthorised copying, use, modification or distribution is strictly prohibited.
 */

#pragma once

// Isolates every #ifdef _WIN32 branch in one place. Nothing outside this
// header should ever check _WIN32 for socket-related code.

#if defined(_WIN32)
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")

namespace nexus::net::platform {

using socket_handle = SOCKET;
inline constexpr socket_handle invalid_socket = INVALID_SOCKET;

inline void close_socket(socket_handle s) { closesocket(s); }

// TCP by itself never notices an idle dead peer - it only detects loss
// when actively sending data and getting no ACK after retries. A
// connection that goes quiet (e.g. a SUBSCRIBE connection between
// events) with no FIN ever received (process killed abruptly enough
// that cleanup didn't run, network drop, ...) can sit "connected"
// indefinitely without this. Windows' SO_KEEPALIVE default timers are
// useless on their own (2hr idle before the first probe) - WSAIoctl
// with SIO_KEEPALIVE_VALS is required to get a timeout actually worth
// having: probe after 5s idle, retry every 2s.
inline void enable_keepalive(socket_handle handle) {
    BOOL keepalive = TRUE;
    setsockopt(handle, SOL_SOCKET, SO_KEEPALIVE,
               reinterpret_cast<const char*>(&keepalive), sizeof(keepalive));

    tcp_keepalive settings{};
    settings.onoff = 1;
    settings.keepalivetime = 5000;      // ms idle before first probe
    settings.keepaliveinterval = 2000;  // ms between probes
    DWORD bytes_returned = 0;
    WSAIoctl(handle, SIO_KEEPALIVE_VALS, &settings, sizeof(settings),
             nullptr, 0, &bytes_returned, nullptr, nullptr);
}

// RAII global Winsock init — constructed once, before any socket use.
struct WinsockInit {
    WinsockInit() {
        WSADATA data;
        WSAStartup(MAKEWORD(2, 2), &data);
    }
    ~WinsockInit() { WSACleanup(); }
};
inline const WinsockInit winsock_init{};

} // namespace nexus::net::platform

#else
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <sys/socket.h>
    #include <unistd.h>

namespace nexus::net::platform {

using socket_handle = int;
inline constexpr socket_handle invalid_socket = -1;

inline void close_socket(socket_handle s) { ::close(s); }

// See the Windows branch above for why this matters. Linux's
// TCP_KEEPIDLE/TCP_KEEPINTVL/TCP_KEEPCNT are the direct POSIX
// equivalent of Windows' SIO_KEEPALIVE_VALS - probe after 5s idle,
// retry every 2s, give up after 3 missed probes.
inline void enable_keepalive(socket_handle handle) {
    int keepalive = 1;
    setsockopt(handle, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive));
#ifdef __linux__
    int idle = 5, interval = 2, count = 3;
    setsockopt(handle, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
    setsockopt(handle, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval));
    setsockopt(handle, IPPROTO_TCP, TCP_KEEPCNT, &count, sizeof(count));
#endif
}

} // namespace nexus::net::platform

#endif
