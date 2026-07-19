#include "nexus/net/socket.hpp"
#include "nexus/protocol/codec.hpp"
#include <iostream>

using namespace nexus;

int main() {
    auto socket = net::Socket::connect("127.0.0.1", 7878);
    if (!socket) {
        std::cerr << "Connection failed\n";
        return 1;
    }
    std::cout << "[insert_client] connected\n";
    
    // Insert row
    std::string insert_cmd = "INSERT transactions tx-1 amount=i:150;currency=s:USD";
    socket->send_line(insert_cmd);
    
    auto response = socket->recv_line();
    std::cout << "[insert_client] response: " << *response << '\n';
    
    socket->close();
    return 0;
}