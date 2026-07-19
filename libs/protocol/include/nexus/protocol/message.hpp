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

using Request = std::variant<SelectRequest, InsertRequest, SubscribeRequest, TransitionRequest>;

} // namespace nexus::protocol
