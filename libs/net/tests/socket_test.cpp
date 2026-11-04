/*
 * Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
 * This file is part of the GrepX platform and is proprietary and confidential.
 * Unauthorised copying, use, modification or distribution is strictly prohibited.
 */

#include "nexus/net/socket.hpp"
#include "nexus/net/tcp_listener.hpp"
#include <cassert>
#include <iostream>
#include <thread>

using namespace nexus::net;

namespace {

void test_loopback_send_recv() {
    auto listener = TcpListener::listen(0); // port 0 not resolvable without getsockname;
    // use a fixed high port for the MVP test instead — deterministic and simple.
    (void)listener;

    auto listener2 = TcpListener::listen(58231);
    assert(listener2.has_value());

    std::thread client_thread([] {
        auto sock = Socket::connect("127.0.0.1", 58231);
        assert(sock.has_value());
        bool ok = sock->send_line("hello from client");
        assert(ok);
        auto reply = sock->recv_line();
        assert(reply.has_value());
        assert(*reply == "hello from server");
    });

    auto conn = listener2->accept();
    assert(conn.has_value());
    auto line = conn->recv_line();
    assert(line.has_value());
    assert(*line == "hello from client");
    [[maybe_unused]] bool sent = conn->send_line("hello from server");

    client_thread.join();
    std::cout << "test_loopback_send_recv: OK\n";
}

void test_multiple_lines_in_one_recv_buffer() {
    auto listener = TcpListener::listen(58232);
    assert(listener.has_value());

    std::thread client_thread([] {
        auto sock = Socket::connect("127.0.0.1", 58232);
        assert(sock.has_value());
        // Send two lines in one call — recv_line must split them correctly.
        [[maybe_unused]] bool s1 = sock->send_line("line-one");
        [[maybe_unused]] bool s2 = sock->send_line("line-two");
    });

    auto conn = listener->accept();
    assert(conn.has_value());
    auto first = conn->recv_line();
    auto second = conn->recv_line();
    assert(first.has_value() && *first == "line-one");
    assert(second.has_value() && *second == "line-two");

    client_thread.join();
    std::cout << "test_multiple_lines_in_one_recv_buffer: OK\n";
}

} // namespace

int main() {
    test_loopback_send_recv();
    test_multiple_lines_in_one_recv_buffer();
    std::cout << "net: all tests passed\n";
    return 0;
}
