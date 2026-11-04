/*
 * Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
 * This file is part of the GrepX platform and is proprietary and confidential.
 * Unauthorised copying, use, modification or distribution is strictly prohibited.
 */

#pragma once
#include "nexus/domain/error.hpp"
#include "nexus/domain/row.hpp"
#include "nexus/domain/row_key.hpp"
#include "nexus/domain/table_name.hpp"
#include <cstdint>

namespace nexus::ports {

// Optimistic-lock update capability. Deliberately its own concept, not
// folded into SnapshotWriter (ISP) — InsertService only ever needs
// unconditional upsert; only TransitionService needs compare-and-swap.
// A writer that can't do CAS fails to compile at the TransitionService
// call site instead of failing at runtime.
template <typename T>
concept ConditionalWriter = requires(T& writer, const domain::TableName& table,
                                      const domain::RowKey& key,
                                      std::uint64_t expected_version, domain::RowPtr row) {
    // Fails with Error::VersionConflict if the row's current version !=
    // expected_version. Returns the new version on success.
    { writer.compare_and_swap(table, key, expected_version, row) }
        -> std::same_as<domain::Expected<std::uint64_t>>;
};

} // namespace nexus::ports
