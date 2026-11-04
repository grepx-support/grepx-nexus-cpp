/*
 * Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
 * This file is part of the GrepX platform and is proprietary and confidential.
 * Unauthorised copying, use, modification or distribution is strictly prohibited.
 */

#pragma once
#include "nexus/infra/event_bus.hpp"
#include "nexus/ports/event_publisher.hpp"

namespace nexus::traits {

struct EventTraits {
    using Bus = infra::EventBus;
};

template <typename T>
concept EventTraitsConcept = requires {
    typename T::Bus;
} && ports::EventPublisher<typename T::Bus>;

static_assert(EventTraitsConcept<EventTraits>);

} // namespace nexus::traits
