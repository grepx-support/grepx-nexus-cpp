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
