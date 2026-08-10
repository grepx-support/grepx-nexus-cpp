#include "connection.hpp"
#include "nexus/app/app_launcher.hpp"
#include "nexus/net/tcp_listener.hpp"
#include "nexus/persistence/sqlite_snapshot_traits.hpp"
#include "nexus/logging/composite_logger.hpp"
#include "nexus/logging/console_logger.hpp"
#include "nexus/logging/file_logger.hpp"
#include "nexus/logging/global_logger.hpp"
#include "nexus/persistence/executable_dir.hpp"
#include "nexus/protocol/codec.hpp"
#include "nexus/session/session.hpp"
#include "nexus/traits/production_traits.hpp"
#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <thread>
#include <vector>

using namespace nexus;

namespace {
    constexpr unsigned short kPort = 7878;

    // Console output alone disappears the moment the terminal is closed
    // or scrolled past — every REQUEST/RESPONSE line this server prints
    // (see handle_connection's `send` lambda) also needs to land
    // somewhere durable for after-the-fact debugging. CompositeLogger
    // forwards to both, so nothing about the console experience changes.
    //
    // Path resolution mirrors SqliteSnapshotStore's DB path (see
    // nexus/persistence/sqlite_snapshot_store.hpp): NEXUS_LOG_PATH env
    // var if set, else "logs/nexus_server.log" next to the executable
    // itself (not the process's CWD, which varies by how it's launched).
    void setup_logging() {
        std::string log_path;
        if (const char *env = std::getenv("NEXUS_LOG_PATH")) {
            log_path = env;
        } else {
            log_path = persistence::executable_dir() + "/logs/nexus_server.log";
        }

        // FileLogger's constructor can throw (bad path, permissions,
        // read-only filesystem, ...). This runs before anything else in
        // main() — an uncaught exception here means std::terminate()
        // before a single line of output, anywhere, which looks exactly
        // like "the server silently exited for no reason". Logging
        // setup failing must never take the whole process down with it;
        // fall back to console-only and say why.
        try {
            std::vector<std::unique_ptr<logging::ILogger>> loggers;
            loggers.push_back(std::make_unique<logging::ConsoleLogger>());
            loggers.push_back(std::make_unique<logging::FileLogger>(log_path));
            logging::set_logger(std::make_unique<logging::CompositeLogger>(std::move(loggers)));
            logging::log().info("log file: " + std::filesystem::absolute(log_path).string());
        } catch (const std::exception &e) {
            logging::set_logger(std::make_unique<logging::ConsoleLogger>());
            logging::log().warn(
                "could not open log file '" + log_path + "' (" + e.what() + ") — console-only for this run");
        }
    }

    // Every accepted connection gets a small sequential id, purely for
    // log correlation — with several connections logging concurrently
    // (see ConsoleLogger's shared mutex), "conn=7" ties a REQUEST line
    // to its RESPONSE line without needing thread ids or timestamps
    // close enough together to eyeball.
    std::atomic<std::uint64_t> next_connection_id{1};

    template<typename Engine>
    void handle_connection(server::ConnectionPtr connection, Engine &engine, std::uint64_t conn_id) {
        // application_name and current_key are mutable and captured by
        // reference below (not passed as parameters) specifically so
        // every log line for the rest of this connection's life — REQUEST,
        // RESPONSE, disconnect — can include them without threading two
        // extra arguments through every call site. application_name is
        // set once, at login; current_key is overwritten each loop
        // iteration from whatever request just came in (only
        // TransitionRequest/SubmitRequest carry one — see key_of() below;
        // everything else logs "-").
        std::string application_name = "-";
        std::string current_key = "-";

        // conn=/app=/key= prefix shared by every per-connection log line —
        // one place to grep on later ("app=trade-demo-backend-worker",
        // "key=<transaction id>", or "conn=7" to follow one connection's
        // whole conversation regardless of which application/key each
        // individual line happens to carry).
        auto ctx = [&]() {
            return "conn=" + std::to_string(conn_id) + " app=" + application_name +
                   " key=" + current_key + " ";
        };

        // Every request line in, every response line out — logged in
        // full at Info level, tagged with conn_id/app/key, for exactly
        // the kind of "what did the client actually send / what did we
        // actually reply" debugging that summarized logs don't answer.
        auto send = [&](const std::string &line) {
            logging::log().info(ctx() + "RESPONSE: " + line);
            return connection->send_line(line);
        };

        bool authenticated = false;
        // Set only once login() succeeds; RAII guard below logs the
        // session out on every exit path (clean disconnect, parse
        // error loop exit, or an exception unwinding the stack) so a
        // crashed/killed connection can never leave a stale "logged
        // in" row blocking that application_name forever.
        std::optional<session::SessionId> session_id;
        struct LogoutGuard {
            Engine &engine;
            std::optional<session::SessionId> &session_id;
            ~LogoutGuard() { if (session_id) engine.sessions().logout(*session_id); }
        } logout_guard{engine, session_id};

        while (true) {
            auto line = connection->recv_line();
            if (!line) {
                logging::log().info(ctx() + "client disconnected");
                return;
            }

            auto request = protocol::parse_request(*line);
            // Only TransitionRequest/SubmitRequest carry a row key (the
            // closest thing this protocol has to a "transaction id") —
            // SELECT/INSERT/SUBSCRIBE/AUTH don't, and log "-" for it.
            current_key = "-";
            if (request) {
                if (auto *r = std::get_if<protocol::TransitionRequest>(&*request)) current_key = r->key;
                else if (auto *r = std::get_if<protocol::SubmitRequest>(&*request)) current_key = r->key;
            }
            logging::log().info(ctx() + "REQUEST: " + *line);

            if (!request) {
                (void) send(protocol::encode_err("could not parse request"));
                continue;
            }
            if (!authenticated) {
                if (auto *auth_req = std::get_if<protocol::AuthRequest>(&*request)) {
                    bool password_ok = engine.authenticator().authenticate(auth_req->username, auth_req->password);
                    if (!password_ok) {
                        logging::log().warn(ctx() + "login failed (bad credentials): user=" + auth_req->username);
                        (void) send(protocol::encode_err("unauthorized"));
                        continue;
                    }
                    auto id = engine.sessions().login(
                        session::Session{auth_req->username, auth_req->application_name, auth_req->access_mode});
                    if (!id) {
                        logging::log().warn(
                            ctx() + "login rejected: application='" + auth_req->application_name +
                            "' access_mode='" + auth_req->access_mode + "' already active");
                        (void) send(
                            protocol::encode_err("application '" + auth_req->application_name + "' already logged in"));
                        continue;
                    }
                    session_id = id;
                    authenticated = true;
                    // Set after login succeeds — every REQUEST/RESPONSE
                    // line for the rest of this connection's life now
                    // carries it via ctx(), not just this one login line.
                    application_name = auth_req->application_name;
                    logging::log().info(
                        ctx() + "login: user=" + auth_req->username + " application=" + auth_req->application_name +
                        " access_mode=" + auth_req->access_mode);
                    (void) send(protocol::encode_ok());
                } else {
                    (void) send(protocol::encode_err("authentication required"));
                }
                continue;
            }
            // A single request's adapter code (SQLite, e.g.) can throw —
            // schema mismatches, disk errors, etc. That must fail just this
            // one request, not bring down every other connection on the
            // process via std::terminate(). Catch at this boundary: one
            // request in, one response out, whatever happens in between.
            try {
                std::visit(
                    [&]<typename Req>(const Req &req) {
                        if constexpr (std::is_same_v<Req, protocol::SelectRequest>) {
                            auto predicate = engine.criteria_parser().parse(req.criteria);
                            if (!predicate) {
                                (void) send(protocol::encode_err("bad criteria"));
                                return;
                            }
                            for (const auto &row: engine.select().execute(req.table, *predicate)) {
                                (void) send(protocol::encode_row(row));
                            }
                            (void) send(protocol::encode_end());
                        } else if constexpr (std::is_same_v<Req, protocol::InsertRequest>) {
                            auto result = engine.insert().execute(req.table, req.row);
                            (void) send(result
                                            ? protocol::encode_ok()
                                            : protocol::encode_err("insert failed"));
                        } else if constexpr (std::is_same_v<Req, protocol::SubscribeRequest>) {
                            // weak_ptr: if the client later disconnects, lock() fails
                            // and we simply skip the send. See connection.hpp.
                            std::weak_ptr<server::Connection> weak_connection = connection;
                            // Captured by value (application_name copied
                            // into app_at_subscribe): this callback can
                            // fire long after handle_connection's own
                            // stack frame has moved on to later requests,
                            // so it can't safely capture application_name
                            // by reference the way ctx()/send() do.
                            [[maybe_unused]] std::string app_at_subscribe = application_name;
                            auto result = engine.subscribe().execute(
                                req.subscription_id, req.table, req.criteria,
                                [weak_connection, conn_id, app_at_subscribe](const domain::ChangeEvent &event) {
                                    if (auto conn = weak_connection.lock()) {
                                        std::string encoded = protocol::encode_event(event);
                                        logging::log().info(
                                            "conn=" + std::to_string(conn_id) + " app=" + app_at_subscribe +
                                            " key=- RESPONSE (event): " + encoded);
                                        (void) conn->send_line(encoded);
                                    }
                                });
                            (void) send(result
                                            ? protocol::encode_ok()
                                            : protocol::encode_err("bad criteria"));
                        } else if constexpr (std::is_same_v<Req, protocol::TransitionRequest>) {
                            auto result = engine.transition().execute(
                                req.table, domain::RowKey{req.key}, req.from_status, req.to_status,
                                req.expected_version, req.row);
                            if (result) {
                                (void) send(protocol::encode_ok(std::to_string(*result)));
                            } else if (result.error() == domain::Error::InvalidTransition) {
                                (void) send(protocol::encode_err("invalid transition"));
                            } else if (result.error() == domain::Error::VersionConflict) {
                                (void) send(protocol::encode_err("version conflict"));
                            } else {
                                (void) send(protocol::encode_err("transition failed"));
                            }
                        } else if constexpr (std::is_same_v<Req, protocol::SubmitRequest>) {
                            // One call does everything TRANSITION used to need two
                            // separate client-driven steps for (see SubmitService):
                            // the server reads the row's current status itself, so
                            // there's no from_status/expected_version for the client
                            // to supply or get wrong.
                            auto result = engine.submit().execute(req.table, domain::RowKey{req.key}, req.row);
                            if (result) {
                                (void) send(protocol::encode_ok(std::to_string(result->version)));
                            } else if (result.error() == domain::Error::InvalidTransition) {
                                (void) send(protocol::encode_err("invalid transition"));
                            } else {
                                (void) send(protocol::encode_err("submit failed"));
                            }
                        } else if constexpr (std::is_same_v<Req, protocol::LogoutRequest>) {
                            // Explicit goodbye: log the session out NOW
                            // rather than waiting for recv_line() to
                            // eventually notice the socket died. Reset
                            // session_id so LogoutGuard's own unconditional
                            // logout() on the way out is a no-op (harmless
                            // either way - logout() is idempotent - but
                            // this keeps the intent explicit rather than
                            // leaning on that as a safety net).
                            if (session_id) {
                                engine.sessions().logout(*session_id);
                                session_id.reset();
                            }
                            (void) send(protocol::encode_ok());
                        }
                    },
                    *request);
            } catch (const std::exception &e) {
                logging::log().error(ctx() + "request failed: " + e.what());
                (void) send(protocol::encode_err(std::string("internal error: ") + e.what()));
            } catch (...) {
                logging::log().error(ctx() + "request failed: unknown exception");
                (void) send(protocol::encode_err("internal error"));
            }

            if (std::holds_alternative<protocol::LogoutRequest>(*request)) {
                logging::log().info(ctx() + "client logged out");
                return;
            }
        }
    }

    // The server "runner" — same shape as nexus_demo's run_console_demo,
    // but drives real network requests instead of a canned script. This is
    // the piece that's actually specific to being a server; the wiring
    // above it (AppEngine<ProductionTraits>) is identical to the demo.
    template<typename Engine>
    int run_server(Engine &engine, int /*argc*/, char * /*argv*/[]) {
        // Printed unconditionally, first thing — this is the #1 "where did
        // it put the file" question. Absolute path, not the possibly-
        // relative one passed to SqliteSnapshotStore, so there's no
        // ambiguity about which CWD it resolved against.
        {
            std::ostringstream oss;
            oss << "db: " << std::filesystem::absolute(engine.store().path());
            logging::log().info(oss.str());
        }

        // SessionRegistry starts empty every process - any row this
        // process's login_audit still considers LOGGED_IN belongs to a
        // connection whose LogoutGuard never ran (server killed/crashed/
        // PC restarted rather than stopped gracefully via stop_nexus_server.sh).
        // Reconcile once, before accepting any new connections, so the
        // audit trail doesn't claim sessions are active forever after a
        // hard restart.
        engine.login_audit().reconcile_orphaned_sessions();

        auto listener = net::TcpListener::listen(kPort);
        if (!listener) {
            logging::log().error("failed to bind port " + std::to_string(kPort));
            return 1;
        }
        logging::log().info("listening on 127.0.0.1:" + std::to_string(kPort));

        std::vector<std::jthread> connection_threads;
        while (true) {
            auto socket = listener->accept();
            if (!socket) continue;

            std::uint64_t conn_id = next_connection_id++;
            logging::log().info("conn=" + std::to_string(conn_id) + " client connected");
            auto connection = std::make_shared<server::Connection>(std::move(*socket));
            connection_threads.emplace_back(
                [connection, &engine, conn_id] { handle_connection(connection, engine, conn_id); });
        }
    }
} // namespace

int main(int argc, char *argv[]) {
    setup_logging();

    using SnapshotTraits = nexus::traits::SqliteSnapshotTraits; // <-- the whole swap
    using AuditTraits = nexus::traits::AuditTraits;
    using EventTraits = nexus::traits::EventTraits;
    using SubscriptionTraits = nexus::traits::SubscriptionTraits;
    using CriteriaTraits = nexus::traits::CriteriaTraits;
    using AuthTraits = traits::AuthTraits;
    using SessionTraits = traits::SessionTraits;
    using ProductionTraits =
            nexus::traits::ProductionTraits<SnapshotTraits, AuditTraits, EventTraits,
                SubscriptionTraits, CriteriaTraits, AuthTraits, SessionTraits>;

    // AppEngine's constructor (inside launch_app) opens the SQLite DB,
    // the authenticator, and the login-audit store — any of which can
    // throw (locked file, bad path, permissions). Without this, an
    // uncaught exception here means std::terminate() before a single
    // line of output — looks exactly like "the server silently exited",
    // which is a much harder bug to diagnose than a logged error and a
    // non-zero exit code.
    try {
        return nexus::app::launch_app<ProductionTraits>(argc, argv, run_server<app::AppEngine<ProductionTraits> >);
    } catch (const std::exception &e) {
        logging::log().error(std::string("startup failed: ") + e.what());
        return 1;
    } catch (...) {
        logging::log().error("startup failed: unknown exception");
        return 1;
    }
}
