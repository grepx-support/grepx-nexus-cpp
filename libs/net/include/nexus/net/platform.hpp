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
    #include <sys/socket.h>
    #include <unistd.h>

namespace nexus::net::platform {

using socket_handle = int;
inline constexpr socket_handle invalid_socket = -1;

inline void close_socket(socket_handle s) { ::close(s); }

} // namespace nexus::net::platform

#endif
