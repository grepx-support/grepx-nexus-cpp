#pragma once
#include "nexus/traits/audit_traits.hpp"
#include "nexus/traits/criteria_traits.hpp"
#include "nexus/traits/event_traits.hpp"
#include "nexus/traits/snapshot_traits.hpp"
#include "nexus/traits/subscription_traits.hpp"

namespace nexus::traits {

// Aggregates every trait group into the single type AppEngine is
// instantiated with. Swapping an implementation (e.g. a Redis-backed
// SnapshotTraits::Store for a future distributed deployment) means
// writing a new *Traits struct and changing one alias here — nothing
// downstream changes.
template <SnapshotTraitsConcept Snapshot, AuditTraitsConcept Audit,
          EventTraitsConcept Event, SubscriptionTraitsConcept Subscription,
          CriteriaTraitsConcept Criteria>
struct ProductionTraits {
    using Store = Snapshot::Store;
    using Log = Audit::Log;
    using Bus = Event::Bus;
    using Registry = Subscription::Registry;
    using Parser = Criteria::Parser;
};

} // namespace nexus::traits
