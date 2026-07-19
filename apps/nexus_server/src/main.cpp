#include "connection.hpp"
#include "nexus/app/app_launcher.hpp"
#include "nexus/net/tcp_listener.hpp"
#include "nexus/persistence/sqlite_snapshot_traits.hpp"
#include "nexus/protocol/codec.hpp"
#include "nexus/traits/production_traits.hpp"
#include <filesystem>
#include <iostream>
#include <thread>
#include <vector>

using namespace nexus;

namespace {

constexpr unsigned short kPort = 7878;

template <typename Engine>
void handle_connection(server::ConnectionPtr connection, Engine& engine) {
    while (true) {
        auto line = connection->recv_line();
        if (!line) {
            std::cout << "[server] client disconnected" << std::endl;
            return;
        }

        auto request = protocol::parse_request(*line);
        if (!request) {
            (void)connection->send_line(protocol::encode_err("could not parse request"));
            continue;
        }

        std::visit(
            [&]<typename Req>(const Req& req) {
                if constexpr (std::is_same_v<Req, protocol::SelectRequest>) {
                    auto predicate = engine.criteria_parser().parse(req.criteria);
                    if (!predicate) {
                        (void)connection->send_line(protocol::encode_err("bad criteria"));
                        return;
                    }
                    for (const auto& row : engine.select().execute(req.table, *predicate)) {
                        (void)connection->send_line(protocol::encode_row(row));
                    }
                    (void)connection->send_line(protocol::encode_end());

                } else if constexpr (std::is_same_v<Req, protocol::InsertRequest>) {
                    auto result = engine.insert().execute(req.table, req.row);
                    (void)connection->send_line(result ? protocol::encode_ok()
                                                         : protocol::encode_err("insert failed"));

                } else if constexpr (std::is_same_v<Req, protocol::SubscribeRequest>) {
                    // weak_ptr: if the client later disconnects, lock() fails
                    // and we simply skip the send. See connection.hpp.
                    std::weak_ptr<server::Connection> weak_connection = connection;
                    auto result = engine.subscribe().execute(
                        req.subscription_id, req.table, req.criteria,
                        [weak_connection](const domain::ChangeEvent& event) {
                            if (auto conn = weak_connection.lock()) {
                                (void)conn->send_line(protocol::encode_event(event));
                            }
                        });
                    (void)connection->send_line(result ? protocol::encode_ok()
                                                         : protocol::encode_err("bad criteria"));

                } else if constexpr (std::is_same_v<Req, protocol::TransitionRequest>) {
                    auto result = engine.transition().execute(
                        req.table, domain::RowKey{req.key}, req.from_status, req.to_status,
                        req.expected_version, req.row);
                    if (result) {
                        (void)connection->send_line(protocol::encode_ok(std::to_string(*result)));
                    } else if (result.error() == domain::Error::InvalidTransition) {
                        (void)connection->send_line(protocol::encode_err("invalid transition"));
                    } else if (result.error() == domain::Error::VersionConflict) {
                        (void)connection->send_line(protocol::encode_err("version conflict"));
                    } else {
                        (void)connection->send_line(protocol::encode_err("transition failed"));
                    }
                }
            },
            *request);
    }
}

// The server "runner" — same shape as nexus_demo's run_console_demo,
// but drives real network requests instead of a canned script. This is
// the piece that's actually specific to being a server; the wiring
// above it (AppEngine<ProductionTraits>) is identical to the demo.
template <typename Engine>
int run_server(Engine& engine, int /*argc*/, char* /*argv*/[]) {
    // Printed unconditionally, first thing — this is the #1 "where did
    // it put the file" question. Absolute path, not the possibly-
    // relative one passed to SqliteSnapshotStore, so there's no
    // ambiguity about which CWD it resolved against.
    std::cout << "[server] db: " << std::filesystem::absolute(engine.store().path()) << std::endl;

    auto listener = net::TcpListener::listen(kPort);
    if (!listener) {
        std::cerr << "failed to bind port " << kPort << '\n';
        return 1;
    }
    std::cout << "[server] listening on 127.0.0.1:" << kPort << std::endl;

    std::vector<std::jthread> connection_threads;
    while (true) {
        auto socket = listener->accept();
        if (!socket) continue;

        std::cout << "[server] client connected" << std::endl;
        auto connection = std::make_shared<server::Connection>(std::move(*socket));
        connection_threads.emplace_back(
            [connection, &engine] { handle_connection(connection, engine); });
    }
}

} // namespace

int main(int argc, char* argv[]) {
    using SnapshotTraits = nexus::traits::SqliteSnapshotTraits; // <-- the whole swap
    using AuditTraits = nexus::traits::AuditTraits;
    using EventTraits = nexus::traits::EventTraits;
    using SubscriptionTraits = nexus::traits::SubscriptionTraits;
    using CriteriaTraits = nexus::traits::CriteriaTraits;
    using ProductionTraits =
        nexus::traits::ProductionTraits<SnapshotTraits, AuditTraits, EventTraits,
                                         SubscriptionTraits, CriteriaTraits>;

    return nexus::app::launch_app<ProductionTraits>(argc, argv, run_server<app::AppEngine<ProductionTraits>>);
}
