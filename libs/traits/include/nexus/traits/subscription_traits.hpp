/*
 * Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
 * This file is part of the GrepX platform and is proprietary and confidential.
 * Unauthorised copying, use, modification or distribution is strictly prohibited.
 */

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
