#pragma once
#include "nexus/domain/error.hpp"
#include "nexus/domain/predicate.hpp"
#include "nexus/domain/row.hpp"
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
        tables_[table][row->key()] = std::move(row);
        return {};
    }

    [[nodiscard]] std::vector<domain::RowPtr>
    select(const domain::TableName& table, const domain::PredicatePtr& predicate) const {
        std::shared_lock lock(mutex_);
        std::vector<domain::RowPtr> results;

        auto it = tables_.find(table);
        if (it == tables_.end()) return results;

        for (const auto& [key, row] : it->second) {
            if (domain::matches(predicate, *row)) results.push_back(row);
        }
        return results;
    }

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<domain::TableName,
                        std::unordered_map<domain::RowKey, domain::RowPtr>>
        tables_;
};

} // namespace nexus::infra
