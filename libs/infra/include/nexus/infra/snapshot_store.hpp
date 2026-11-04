/*
 * Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
 * This file is part of the GrepX platform and is proprietary and confidential.
 * Unauthorised copying, use, modification or distribution is strictly prohibited.
 */

#pragma once
#include "nexus/domain/error.hpp"
#include "nexus/domain/predicate.hpp"
#include "nexus/domain/row.hpp"
#include "nexus/domain/row_key.hpp"
#include "nexus/domain/table_name.hpp"
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

namespace nexus::infra {

// MVP concurrency model: one shared_mutex guarding all tables.
// Phase 2 swap: strand-per-table (see architecture notes) — the
// SnapshotReader/SnapshotWriter interface this satisfies does not change.
class SnapshotStore {
public:
    domain::Expected<void> upsert(const domain::TableName& table, domain::RowPtr row) {
        std::unique_lock lock(mutex_);
        auto& slot = tables_[table][row->key()];
        slot.row = std::move(row);
        ++slot.version;
        return {};
    }

    [[nodiscard]] std::vector<domain::RowPtr>
    select(const domain::TableName& table, const domain::PredicatePtr& predicate) const {
        std::shared_lock lock(mutex_);
        std::vector<domain::RowPtr> results;

        auto it = tables_.find(table);
        if (it == tables_.end()) return results;

        for (const auto& [key, slot] : it->second) {
            if (domain::matches(predicate, *slot.row)) results.push_back(slot.row);
        }
        return results;
    }

    // ports::ConditionalWriter — optimistic-lock update used by
    // TransitionService. Fails with VersionConflict if expected_version
    // doesn't match the row's current version, RowNotFound if the row
    // (or table) doesn't exist yet.
    domain::Expected<std::uint64_t>
    compare_and_swap(const domain::TableName& table, const domain::RowKey& key,
                      std::uint64_t expected_version, domain::RowPtr row) {
        std::unique_lock lock(mutex_);
        auto table_it = tables_.find(table);
        if (table_it == tables_.end()) return std::unexpected(domain::Error::RowNotFound);

        auto row_it = table_it->second.find(key);
        if (row_it == table_it->second.end()) return std::unexpected(domain::Error::RowNotFound);

        if (row_it->second.version != expected_version) {
            return std::unexpected(domain::Error::VersionConflict);
        }

        row_it->second.row = std::move(row);
        ++row_it->second.version;
        return row_it->second.version;
    }

private:
    struct Slot {
        domain::RowPtr row;
        std::uint64_t version = 0;
    };

    mutable std::shared_mutex mutex_;
    std::unordered_map<domain::TableName, std::unordered_map<domain::RowKey, Slot>> tables_;
};

} // namespace nexus::infra
