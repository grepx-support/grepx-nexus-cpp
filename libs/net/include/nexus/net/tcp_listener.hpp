#pragma once
#include "nexus/net/socket.hpp"

namespace nexus::net {

class TcpListener {
public:
    static std::expected<TcpListener, SocketError> listen(unsigned short port) {
        platform::socket_handle handle = ::socket(AF_INET, SOCK_STREAM, 0);

        int reuse = 1;
        setsockopt(handle, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);

        if (bind(handle, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
            platform::close_socket(handle);
            return std::unexpected(SocketError::BindFailed);
        }
        if (::listen(handle, /*backlog=*/64) != 0) {
            platform::close_socket(handle);
            return std::unexpected(SocketError::ListenFailed);
        }
        return TcpListener(handle);
    }

    TcpListener(const TcpListener&) = delete;
    TcpListener& operator=(const TcpListener&) = delete;
    TcpListener(TcpListener&& other) noexcept : handle_(other.handle_) {
        other.handle_ = platform::invalid_socket;
    }

    ~TcpListener() {
        if (handle_ != platform::invalid_socket) platform::close_socket(handle_);
    }

    // Blocks until a client connects.
    [[nodiscard]] std::expected<Socket, SocketError> accept() {
        sockaddr_in client_addr{};
        socklen_t len = sizeof(client_addr);
        platform::socket_handle client =
            ::accept(handle_, reinterpret_cast<sockaddr*>(&client_addr), &len);
        if (client == platform::invalid_socket) return std::unexpected(SocketError::AcceptFailed);
        return Socket(client);
    }

private:
    explicit TcpListener(platform::socket_handle handle) : handle_(handle) {}
    platform::socket_handle handle_;
};

} // namespace nexus::net
