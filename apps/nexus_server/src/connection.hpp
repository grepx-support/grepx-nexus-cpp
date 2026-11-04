/*
 * Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
 * This file is part of the GrepX platform and is proprietary and confidential.
 * Unauthorised copying, use, modification or distribution is strictly prohibited.
 */

#pragma once
#include "nexus/net/socket.hpp"
#include <memory>
#include <mutex>
#include <string>

namespace nexus::server {

// Wraps one client's socket with a mutex around sends, since a notify
// can arrive from a different connection's thread (whichever thread did
// the INSERT) while this connection's own thread is blocked in recv.
// Held by subscriptions as a weak_ptr — see DispatchService wiring in
// main.cpp — so a disconnected client's dangling socket is never
// written to.
class Connection : public std::enable_shared_from_this<Connection> {
public:
    explicit Connection(net::Socket socket) : socket_(std::move(socket)) {}

    [[nodiscard]] bool send_line(const std::string& line) {
        std::lock_guard lock(send_mutex_);
        return socket_.send_line(line);
    }

    [[nodiscard]] std::optional<std::string> recv_line() { return socket_.recv_line(); }

private:
    net::Socket socket_;
    std::mutex send_mutex_;
};

using ConnectionPtr = std::shared_ptr<Connection>;

} // namespace nexus::server
