/*
 * Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
 * This file is part of the GrepX platform and is proprietary and confidential.
 * Unauthorised copying, use, modification or distribution is strictly prohibited.
 */

// Nexus client — a standalone process. Depends only on nexus_net and
// nexus_protocol: no knowledge of domain internals, no linkage against
// nexus_infra or nexus_application. This is the loose-coupling proof —
// a client could be rewritten in any language against the same wire
// protocol without touching the server.
#include "nexus/net/socket.hpp"
#include "nexus/protocol/codec.hpp"
#include <iostream>
#include <sstream>
#include <thread>

using namespace nexus;

namespace {

constexpr unsigned short kPort = 7878;

// Background reader thread — prints every line the server sends
// (SELECT results, EVENT notifications) as it arrives.
void reader_loop(net::Socket& socket) {
    while (auto line = socket.recv_line()) {
        std::cout << "< " << *line << '\n';
    }
    std::cout << "[client] server closed the connection\n";
}

} // namespace

int main(int argc, char* argv[]) {
    std::string host = argc > 1 ? argv[1] : "127.0.0.1";

    auto socket = net::Socket::connect(host, kPort);
    if (!socket) {
        std::cerr << "failed to connect to " << host << ':' << kPort << '\n';
        return 1;
    }
    std::cout << "[client] connected to " << host << ':' << kPort << '\n';
    std::cout << "commands:\n"
              << "  select <table> <criteria...>\n"
              << "  insert <table> <key> field=type:value;...   (types: i d b s n)\n"
              << "  subscribe <id> <table> <criteria...>\n"
              << "  quit\n\n";

    std::jthread reader(reader_loop, std::ref(*socket));

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        if (line == "quit") break;

        std::istringstream iss(line);
        std::string verb;
        iss >> verb;
        std::string rest;
        std::getline(iss, rest);
        if (!rest.empty() && rest.front() == ' ') rest.erase(0, 1);

        std::string wire_line;
        if (verb == "select") wire_line = "SELECT " + rest;
        else if (verb == "insert") wire_line = "INSERT " + rest;
        else if (verb == "subscribe") wire_line = "SUBSCRIBE " + rest;
        else {
            std::cout << "unknown command: " << verb << '\n';
            continue;
        }

        if (!socket->send_line(wire_line)) {
            std::cerr << "[client] send failed, connection lost\n";
            break;
        }
    }

    socket->close();
    return 0;
}
