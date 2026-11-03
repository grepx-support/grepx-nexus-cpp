/*
 * Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
 * This file is part of the GrepX platform and is proprietary and confidential.
 * Unauthorised copying, use, modification or distribution is strictly prohibited.
 */

#include "nexus/net/socket.hpp"
#include <iostream>
#include <thread>

using namespace nexus;

void reader_loop(net::Socket& socket) {
    while (auto line = socket.recv_line()) {
        std::cout << "[notify_client_b] received: " << *line << '\n';
    }
}

int main() {
    auto socket = net::Socket::connect("127.0.0.1", 7878);
    if (!socket) {
        std::cerr << "Connection failed\n";
        return 1;
    }
    std::cout << "[notify_client_b] connected, subscribing to currency = 'USD'...\n";
    
    // Subscribe
    socket->send_line("SUBSCRIBE sub-b transactions currency = 'USD'");
    auto response = socket->recv_line();
    std::cout << "[notify_client_b] " << *response << '\n';
    
    std::cout << "[notify_client_b] waiting for notifications...\n";
    reader_loop(*socket);
    
    socket->close();
    return 0;
}