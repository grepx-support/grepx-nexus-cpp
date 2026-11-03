/*
 * Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
 * This file is part of the GrepX platform and is proprietary and confidential.
 * Unauthorised copying, use, modification or distribution is strictly prohibited.
 */

#pragma once
#include "nexus/domain/row.hpp"
#include "nexus/domain/table_name.hpp"
#include <chrono>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace nexus::infra {

struct AuditEntry {
    std::uint64_t sequence;
    domain::RowPtr row;
    std::chrono::system_clock::time_point recorded_at;
};

// Append-only. No method ever mutates or removes a past entry.
class AuditLog {
public:
    void append(const domain::TableName& table, domain::RowPtr row) {
        std::lock_guard lock(mutex_);
        auto& entries = entries_[table];
        entries.push_back(AuditEntry{
            .sequence = next_sequence_++,
            .row = std::move(row),
            .recorded_at = std::chrono::system_clock::now(),
        });
    }

    [[nodiscard]] std::size_t count(const domain::TableName& table) const {
        std::lock_guard lock(mutex_);
        auto it = entries_.find(table);
        return it == entries_.end() ? 0 : it->second.size();
    }

private:
    mutable std::mutex mutex_;
    std::uint64_t next_sequence_ = 0;
    std::unordered_map<domain::TableName, std::vector<AuditEntry>> entries_;
};

} // namespace nexus::infra
