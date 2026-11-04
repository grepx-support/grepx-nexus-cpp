/*
 * Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
 * This file is part of the GrepX platform and is proprietary and confidential.
 * Unauthorised copying, use, modification or distribution is strictly prohibited.
 */

#pragma once
#include "nexus/ports/subscription_registry.hpp"
#include <algorithm>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace nexus::infra {

class SubscriptionRegistry {
public:
    void add(ports::Subscription subscription) {
        std::lock_guard lock(mutex_);
        by_table_[subscription.table].push_back(std::move(subscription));
    }

    void remove(const domain::TableName& table, const std::string& id) {
        std::lock_guard lock(mutex_);
        auto it = by_table_.find(table);
        if (it == by_table_.end()) return;

        auto& subs = it->second;
        std::erase_if(subs, [&](const ports::Subscription& s) { return s.id == id; });
    }

    // Returns a COPY, not a reference. Returning a reference into the
    // internal vector after releasing the lock let a concurrent add()
    // reallocate the vector out from under a caller mid-iteration — a
    // real data race, caught under ThreadSanitizer. Subscription is
    // cheap to copy (an id, a table name, a shared_ptr predicate, and a
    // std::function), so paying for the copy here is the right trade.
    [[nodiscard]] std::vector<ports::Subscription> for_table(const domain::TableName& table) const {
        std::lock_guard lock(mutex_);
        auto it = by_table_.find(table);
        return it == by_table_.end() ? std::vector<ports::Subscription>{} : it->second;
    }

private:
    mutable std::mutex mutex_;
    std::unordered_map<domain::TableName, std::vector<ports::Subscription>> by_table_;
};

} // namespace nexus::infra
