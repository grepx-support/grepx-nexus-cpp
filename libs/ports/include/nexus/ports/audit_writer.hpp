#pragma once
#include "nexus/domain/row.hpp"
#include "nexus/domain/table_name.hpp"

namespace nexus::ports {

// Append-only audit trail. No read method — audit is a write-side
// record only in this MVP (see architecture note on event sourcing).
template <typename T>
concept AuditWriter = requires(T& auditor, const domain::TableName& table,
                                domain::RowPtr row) {
    { auditor.append(table, row) } -> std::same_as<void>;
};

} // namespace nexus::ports
