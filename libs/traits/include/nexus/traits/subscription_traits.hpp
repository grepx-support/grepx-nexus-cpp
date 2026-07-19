#pragma once
#include "nexus/infra/subscription_registry.hpp"
#include "nexus/ports/subscription_registry.hpp"

namespace nexus::traits {

struct SubscriptionTraits {
    using Registry = infra::SubscriptionRegistry;
};

template <typename T>
concept SubscriptionTraitsConcept = requires {
    typename T::Registry;
} && ports::SubscriptionRegistry<typename T::Registry>;

static_assert(SubscriptionTraitsConcept<SubscriptionTraits>);

} // namespace nexus::traits
