/*
 * Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
 * This file is part of the GrepX platform and is proprietary and confidential.
 * Unauthorised copying, use, modification or distribution is strictly prohibited.
 */

#pragma once
#include "nexus/net/platform.hpp"
#include <expected>
#include <optional>
#include <string>
#include <utility>

namespace nexus::net {

enum class SocketError { ConnectFailed, BindFailed, ListenFailed, AcceptFailed, SendFailed };

// Move-only RAII wrapper around one connected TCP socket. Line-buffered:
// recv_line() blocks until a full '\n'-terminated line has arrived (or
// the peer closes), so callers never see partial messages.
class Socket {
public:
    Socket() = default;
    explicit Socket(platform::socket_handle handle) : handle_(handle) {
        // Applies to every Socket regardless of how it was created -
        // both connect() below and TcpListener::accept() construct
        // through this same constructor. See platform::enable_keepalive
        // for why this matters (detecting a peer that died without
        // sending a FIN, within seconds instead of Windows' multi-minute
        // default TCP timeout).
        if (valid()) platform::enable_keepalive(handle_);
    }

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    Socket(Socket&& other) noexcept : handle_(other.handle_), buffer_(std::move(other.buffer_)) {
        other.handle_ = platform::invalid_socket;
    }
    Socket& operator=(Socket&& other) noexcept {
        if (this != &other) {
            close();
            handle_ = other.handle_;
            buffer_ = std::move(other.buffer_);
            other.handle_ = platform::invalid_socket;
        }
        return *this;
    }

    ~Socket() { close(); }

    [[nodiscard]] bool valid() const noexcept { return handle_ != platform::invalid_socket; }

    static std::expected<Socket, SocketError> connect(const std::string& host, unsigned short port) {
        platform::socket_handle handle = ::socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, host.c_str(), &addr.sin_addr);

        if (::connect(handle, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
            platform::close_socket(handle);
            return std::unexpected(SocketError::ConnectFailed);
        }
        return Socket(handle);
    }

    // Blocking read of one '\n'-terminated line, delimiter stripped.
    // Returns nullopt when the peer closes the connection.
    [[nodiscard]] std::optional<std::string> recv_line() {
        while (true) {
            if (auto pos = buffer_.find('\n'); pos != std::string::npos) {
                std::string line = buffer_.substr(0, pos);
                buffer_.erase(0, pos + 1);
                if (!line.empty() && line.back() == '\r') line.pop_back();
                return line;
            }

            char chunk[4096];
            auto received = recv(handle_, chunk, sizeof(chunk), 0);
            if (received <= 0) return std::nullopt;
            buffer_.append(chunk, static_cast<std::size_t>(received));
        }
    }

    [[nodiscard]] bool send_line(const std::string& line) {
        std::string framed = line + "\n";
        std::size_t total_sent = 0;
        while (total_sent < framed.size()) {
            auto sent = send(handle_, framed.data() + total_sent,
                              static_cast<int>(framed.size() - total_sent), 0);
            if (sent <= 0) return false;
            total_sent += static_cast<std::size_t>(sent);
        }
        return true;
    }

    // shutdown() before close() matters when another thread may be
    // blocked in recv() on this same socket (e.g. a reader thread while
    // the owning thread closes on disconnect/shutdown). Plain close()
    // from another thread while a recv() is in flight is undefined
    // behavior on POSIX and can hang that thread forever; shutdown() is
    // specifically documented to unblock it.
    void close() {
        if (valid()) {
#if defined(_WIN32)
            shutdown(handle_, SD_BOTH);
#else
            ::shutdown(handle_, SHUT_RDWR);
#endif
            platform::close_socket(handle_);
            handle_ = platform::invalid_socket;
        }
    }

    [[nodiscard]] platform::socket_handle native_handle() const noexcept { return handle_; }

private:
    platform::socket_handle handle_ = platform::invalid_socket;
    std::string buffer_;
};

} // namespace nexus::net
