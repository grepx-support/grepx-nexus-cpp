#pragma once
// UNVERIFIED IN THIS SANDBOX — see redis_snapshot_store.hpp for why.
#include "nexus/persistence/redis_snapshot_store.hpp"
#include "nexus/traits/snapshot_traits.hpp"

namespace nexus::traits {

struct RedisSnapshotTraits {
    using Store = persistence::RedisSnapshotStore;
};
// NOTE: this static_assert would confirm RedisSnapshotStore satisfies
// SnapshotTraitsConcept — it's commented out because this header can't
// even be included without hiredis/hiredis.h present, which isn't
// available in this sandbox. Uncomment once hiredis is on your
// include path:
// static_assert(SnapshotTraitsConcept<RedisSnapshotTraits>);

} // namespace nexus::traits
