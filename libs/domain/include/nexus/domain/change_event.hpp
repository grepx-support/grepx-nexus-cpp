#pragma once
#include "nexus/domain/row.hpp"
#include "nexus/domain/table_name.hpp"

namespace nexus::domain {

enum class ChangeKind { Inserted, Updated };

struct ChangeEvent {
    TableName table;
    RowPtr row;
    ChangeKind kind;
};

} // namespace nexus::domain
