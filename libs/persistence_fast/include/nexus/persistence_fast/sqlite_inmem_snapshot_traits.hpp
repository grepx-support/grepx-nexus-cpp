/*
 * Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
 * This file is part of the GrepX platform and is proprietary and confidential.
 * Unauthorised copying, use, modification or distribution is strictly prohibited.
 */

#pragma once
#include "nexus/persistence_fast/sqlite_inmem_store.hpp"
#include "nexus/traits/snapshot_traits.hpp"

namespace nexus::traits {

// Trait bundle that swaps SqliteSnapshotStore for SqliteInMemStore.
// Usage in main.cpp (controlled by -DNEXUS_FAST_STORE=ON at cmake time):
//
//   #if defined(NEXUS_USE_FAST_STORE)
//       using SnapshotTraits = nexus::traits::SqliteInMemSnapshotTraits;
//   #else
//       using SnapshotTraits = nexus::traits::SqliteSnapshotTraits;
//   #endif
struct SqliteInMemSnapshotTraits {
    using Store = persistence_fast::SqliteInMemStore;
};

// Verified at compile time: SqliteInMemSnapshotTraits satisfies the same
// concept that SqliteSnapshotTraits does, so AppEngine<ProductionTraits>
// compiles identically with either store — no other code changes.
static_assert(SnapshotTraitsConcept<SqliteInMemSnapshotTraits>);

} // namespace nexus::traits
