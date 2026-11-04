/*
 * Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
 * This file is part of the GrepX platform and is proprietary and confidential.
 * Unauthorised copying, use, modification or distribution is strictly prohibited.
 */

#pragma once
#include "nexus/domain/change_event.hpp"
#include "nexus/domain/predicate.hpp"
#include "nexus/domain/table_name.hpp"
#include <functional>
#include <string>
#include <vector>

namespace nexus::ports {

struct Subscription {
    std::string id;
    domain::TableName table;
    domain::PredicatePtr predicate;
    std::function<void(const domain::ChangeEvent&)> notify;
};

template <typename T>
concept SubscriptionRegistry = requires(T& registry, Subscription subscription,
                                         const domain::TableName& table,
                                         const std::string& id) {
    { registry.add(std::move(subscription)) } -> std::same_as<void>;
    { registry.remove(table, id) } -> std::same_as<void>;
    { registry.for_table(table) } -> std::same_as<std::vector<Subscription>>;
};

} // namespace nexus::ports
