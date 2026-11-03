/*
 * Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
 * This file is part of the GrepX platform and is proprietary and confidential.
 * Unauthorised copying, use, modification or distribution is strictly prohibited.
 */

#pragma once
#include "nexus/domain/row.hpp"
#include "nexus/domain/table_name.hpp"

namespace nexus::domain {

enum class ChangeKind { Inserted, Updated };

struct ChangeEvent {
    TableName table;
    RowPtr row;
    ChangeKind kind;
};

} // namespace nexus::domain
