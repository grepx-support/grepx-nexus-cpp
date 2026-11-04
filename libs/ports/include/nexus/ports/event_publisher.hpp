/*
 * Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
 * This file is part of the GrepX platform and is proprietary and confidential.
 * Unauthorised copying, use, modification or distribution is strictly prohibited.
 */

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
