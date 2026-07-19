#pragma once
#include "nexus/domain/row.hpp"
#include "nexus/domain/table_name.hpp"
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

using Request = std::variant<SelectRequest, InsertRequest, SubscribeRequest>;

} // namespace nexus::protocol
