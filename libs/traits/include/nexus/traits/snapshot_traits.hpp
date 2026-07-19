#pragma once
#include "nexus/infra/snapshot_store.hpp"
#include "nexus/ports/snapshot_reader.hpp"
#include "nexus/ports/snapshot_writer.hpp"

namespace nexus::traits {

struct SnapshotTraits {
    using Store = infra::SnapshotStore;
};

template <typename T>
concept SnapshotTraitsConcept = requires {
    typename T::Store;
} && ports::SnapshotReader<typename T::Store> && ports::SnapshotWriter<typename T::Store>;

static_assert(SnapshotTraitsConcept<SnapshotTraits>);

} // namespace nexus::traits
