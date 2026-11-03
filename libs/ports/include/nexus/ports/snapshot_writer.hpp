/*
 * Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
 * This file is part of the GrepX platform and is proprietary and confidential.
 * Unauthorised copying, use, modification or distribution is strictly prohibited.
 */

#pragma once
#include "nexus/domain/error.hpp"
#include "nexus/domain/row.hpp"
#include "nexus/domain/table_name.hpp"

namespace nexus::ports {

// Write-only access to the snapshot. Split from SnapshotReader so
// InsertService never gains the ability to read (ISP).
template <typename T>
concept SnapshotWriter = requires(T& writer, const domain::TableName& table,
                                   domain::RowPtr row) {
    { writer.upsert(table, row) } -> std::same_as<domain::Expected<void>>;
};

} // namespace nexus::ports
