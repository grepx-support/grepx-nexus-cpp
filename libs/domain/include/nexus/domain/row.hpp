/*
 * Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
 * This file is part of the GrepX platform and is proprietary and confidential.
 * Unauthorised copying, use, modification or distribution is strictly prohibited.
 */

#pragma once
#include "nexus/domain/row_key.hpp"
#include "nexus/domain/value.hpp"
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace nexus::domain {

// An immutable snapshot of one row's fields. Once constructed a Row never
// changes; updates create a new Row and replace the shared_ptr in the store.
class Row {
public:
    Row(RowKey key, std::unordered_map<std::string, Value> fields)
        : key_(std::move(key)), fields_(std::move(fields)) {}

    [[nodiscard]] const RowKey& key() const noexcept { return key_; }

    [[nodiscard]] const Value* find(std::string_view field) const noexcept {
        auto it = fields_.find(std::string(field));
        return it == fields_.end() ? nullptr : &it->second;
    }

    [[nodiscard]] const std::unordered_map<std::string, Value>& fields() const noexcept {
        return fields_;
    }

private:
    RowKey key_;
    std::unordered_map<std::string, Value> fields_;
};

using RowPtr = std::shared_ptr<const Row>;

} // namespace nexus::domain
