#include "nexus/net/socket.hpp"
#include <iostream>
#include <thread>

using namespace nexus;

void reader_loop(net::Socket& socket) {
    while (auto line = socket.recv_line()) {
        std::cout << "[notify_client_a] received: " << *line << '\n';
    }
}

int main() {
    auto socket = net::Socket::connect("127.0.0.1", 7878);
    if (!socket) {
        std::cerr << "Connection failed\n";
        return 1;
    }
    std::cout << "[notify_client_a] connected, subscribing to amount > 100...\n";

    // Subscribe
    socket->send_line("SUBSCRIBE sub-a transactions amount > 100");
    auto response = socket->recv_line();
    std::cout << "[notify_client_a] " << *response << '\n';
    
    std::cout << "[notify_client_a] waiting for notifications...\n";
    reader_loop(*socket);
    
    socket->close();
    return 0;
}