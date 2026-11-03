/*
 * Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
 * This file is part of the GrepX platform and is proprietary and confidential.
 * Unauthorised copying, use, modification or distribution is strictly prohibited.
 */

#pragma once
#include "nexus/domain/change_event.hpp"
#include "nexus/domain/predicate.hpp"
#include "nexus/ports/subscription_registry.hpp"

namespace nexus::application {

// Wired as the EventBus's handler. For each change event, finds
// subscriptions on the affected table and notifies the ones whose
// predicate matches the changed row.
template <ports::SubscriptionRegistry Registry>
class DispatchService {
public:
    explicit DispatchService(const Registry& registry) : registry_(registry) {}

    void on_event(const domain::ChangeEvent& event) const {
        for (const auto& subscription : registry_.for_table(event.table)) {
            if (domain::matches(subscription.predicate, *event.row)) {
                subscription.notify(event);
            }
        }
    }

private:
    const Registry& registry_;
};

} // namespace nexus::application
