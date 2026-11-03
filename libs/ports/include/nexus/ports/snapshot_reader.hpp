/*
 * Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
 * This file is part of the GrepX platform and is proprietary and confidential.
 * Unauthorised copying, use, modification or distribution is strictly prohibited.
 */

#pragma once
#include "nexus/domain/predicate.hpp"
#include "nexus/domain/row.hpp"
#include "nexus/domain/table_name.hpp"
#include <vector>

namespace nexus::ports {

// Read-only access to the latest snapshot. Deliberately excludes any
// write method so SelectService can never mutate state (ISP).
template <typename T>
concept SnapshotReader = requires(const T& reader, const domain::TableName& table,
                                   const domain::PredicatePtr& predicate) {
    { reader.select(table, predicate) } -> std::same_as<std::vector<domain::RowPtr>>;
};

} // namespace nexus::ports
