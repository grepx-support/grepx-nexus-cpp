/*
 * Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
 * This file is part of the GrepX platform and is proprietary and confidential.
 * Unauthorised copying, use, modification or distribution is strictly prohibited.
 */

#pragma once
#include "nexus/domain/row.hpp"
#include "nexus/domain/table_name.hpp"
#include <cstdint>
#include <string>
#include <variant>

namespace nexus::protocol {
    struct SelectRequest {
        domain::TableName table;
        std::string criteria; // empty = always_true
    };

    struct InsertRequest {
        domain::TableName table;
        domain::RowPtr row;
    };

    struct SubscribeRequest {
        std::string subscription_id;
        domain::TableName table;
        std::string criteria;
    };

    struct TransitionRequest {
        domain::TableName table;
        std::string key;
        std::string from_status;
        std::string to_status;
        std::uint64_t expected_version;
        domain::RowPtr row; // full new field set (status + whatever else changed)
    };

    struct AuthRequest {
        std::string username;
        std::string password;
        std::string application_name; // must be unique among active sessions
        std::string access_mode;      // e.g. "ALL" or "SELECT,SUBSCRIBE" — recorded for audit
    };

    // One business transaction, submitted once. Unlike TransitionRequest,
    // the caller does NOT supply from_status or expected_version — the
    // server reads the row's current state itself, inside the same
    // atomic operation that applies the write (see
    // nexus::application::SubmitService / ports::SubmitWriter), so
    // there's nothing for a client to read first, get stale, or race on.
    // `row` carries the requested status via its own "status" field
    // (same envelope shape client code already builds for INSERT).
    struct SubmitRequest {
        domain::TableName table;
        std::string key;
        domain::RowPtr row;
    };

    // Explicit graceful goodbye - lets the server log a session out the
    // instant a well-behaved client actually intends to stop, instead of
    // relying solely on TCP noticing the socket died (a forceful kill
    // never gets to send this at all - see platform::enable_keepalive
    // for that path's safety net). No fields: one connection, one
    // session, nothing else to say.
    struct LogoutRequest {};

    using Request = std::variant<AuthRequest, SelectRequest, InsertRequest, SubscribeRequest, TransitionRequest,
                                  SubmitRequest, LogoutRequest>;
} // namespace nexus::protocol
