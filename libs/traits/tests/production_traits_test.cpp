#include "nexus/traits/production_traits.hpp"
#include <iostream>

using namespace nexus;

// The production wiring, built entirely from concrete infra/criteria types.
using RealProduction =
    traits::ProductionTraits<traits::SnapshotTraits, traits::AuditTraits, traits::EventTraits,
                              traits::SubscriptionTraits, traits::CriteriaTraits>;

static_assert(std::same_as<RealProduction::Store, infra::SnapshotStore>);
static_assert(std::same_as<RealProduction::Log, infra::AuditLog>);
static_assert(std::same_as<RealProduction::Bus, infra::EventBus>);
static_assert(std::same_as<RealProduction::Registry, infra::SubscriptionRegistry>);
static_assert(std::same_as<RealProduction::Parser, criteria::SqlLikeCriteriaParser>);

namespace {

// A fake snapshot store satisfying SnapshotReader + SnapshotWriter, used
// only to prove ProductionTraits accepts ANY conforming trait group —
// not just the shipped infra implementation. This is the swappability
// the whole trait design exists for.
struct FakeStore {
    domain::Expected<void> upsert(const domain::TableName&, domain::RowPtr) { return {}; }
    std::vector<domain::RowPtr> select(const domain::TableName&, const domain::PredicatePtr&) const {
        return {};
    }
};

struct FakeSnapshotTraits {
    using Store = FakeStore;
};
static_assert(traits::SnapshotTraitsConcept<FakeSnapshotTraits>);

using TestProduction =
    traits::ProductionTraits<FakeSnapshotTraits, traits::AuditTraits, traits::EventTraits,
                              traits::SubscriptionTraits, traits::CriteriaTraits>;
static_assert(std::same_as<TestProduction::Store, FakeStore>);

} // namespace

int main() {
    std::cout << "traits/production_traits: all static_assert checks passed\n";
    return 0;
}
