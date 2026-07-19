#pragma once
#include "nexus/persistence/sqlite_snapshot_store.hpp"
#include "nexus/traits/snapshot_traits.hpp"

namespace nexus::traits {

struct SqliteSnapshotTraits {
    using Store = persistence::SqliteSnapshotStore;
};
static_assert(SnapshotTraitsConcept<SqliteSnapshotTraits>);

} // namespace nexus::traits
