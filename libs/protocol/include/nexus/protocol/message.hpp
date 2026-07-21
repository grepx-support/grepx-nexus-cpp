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

    using Request = std::variant<AuthRequest, SelectRequest, InsertRequest, SubscribeRequest, TransitionRequest>;
} // namespace nexus::protocol
