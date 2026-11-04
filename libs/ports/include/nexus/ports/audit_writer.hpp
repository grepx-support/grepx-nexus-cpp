/*
 * Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
 * This file is part of the GrepX platform and is proprietary and confidential.
 * Unauthorised copying, use, modification or distribution is strictly prohibited.
 */

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
