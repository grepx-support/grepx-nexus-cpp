/*
 * Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
 * This file is part of the GrepX platform and is proprietary and confidential.
 * Unauthorised copying, use, modification or distribution is strictly prohibited.
 */

#pragma once
#include "nexus/persistence/sqlite_snapshot_store.hpp"
#include "nexus/traits/snapshot_traits.hpp"

namespace nexus::traits {

struct SqliteSnapshotTraits {
    using Store = persistence::SqliteSnapshotStore;
};
static_assert(SnapshotTraitsConcept<SqliteSnapshotTraits>);

} // namespace nexus::traits
