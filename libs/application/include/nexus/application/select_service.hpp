/*
 * Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
 * This file is part of the GrepX platform and is proprietary and confidential.
 * Unauthorised copying, use, modification or distribution is strictly prohibited.
 */

#pragma once
#include "nexus/ports/snapshot_reader.hpp"
#include <vector>

namespace nexus::application {

// Depends only on the read side of the snapshot (ISP) — this class has
// no way to mutate state even by accident.
template <ports::SnapshotReader Reader>
class SelectService {
public:
    explicit SelectService(const Reader& reader) : reader_(reader) {}

    [[nodiscard]] std::vector<domain::RowPtr>
    execute(const domain::TableName& table, const domain::PredicatePtr& predicate) const {
        return reader_.select(table, predicate);
    }

private:
    const Reader& reader_;
};

} // namespace nexus::application
