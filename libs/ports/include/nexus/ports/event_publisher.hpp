#pragma once
#include "nexus/domain/change_event.hpp"

namespace nexus::ports {

// Publish-only side of the event bus. InsertService depends on this
// alone — it never subscribes (ISP).
template <typename T>
concept EventPublisher = requires(T& bus, const domain::ChangeEvent& event) {
    { bus.publish(event) } -> std::same_as<void>;
};

} // namespace nexus::ports
