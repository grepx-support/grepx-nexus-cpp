/*
 * Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
 * This file is part of the GrepX platform and is proprietary and confidential.
 * Unauthorised copying, use, modification or distribution is strictly prohibited.
 */

#include "nexus/domain/row.hpp"
#include <cassert>
#include <iostream>

using namespace nexus::domain;

namespace {

void test_row_stores_and_finds_fields() {
    Row row(RowKey{"tx-1"}, {{"amount", Value{std::int64_t{100}}}, {"currency", Value{std::string{"USD"}}}});

    assert(row.key() == RowKey{"tx-1"});
    assert(row.find("amount") != nullptr);
    assert(std::get<std::int64_t>(*row.find("amount")) == 100);
    assert(row.find("missing") == nullptr);
    std::cout << "test_row_stores_and_finds_fields: OK\n";
}

void test_row_key_equality() {
    assert(RowKey{"a"} == RowKey{"a"});
    assert(!(RowKey{"a"} == RowKey{"b"}));
    std::cout << "test_row_key_equality: OK\n";
}

} // namespace

int main() {
    test_row_stores_and_finds_fields();
    test_row_key_equality();
    std::cout << "domain/row: all tests passed\n";
    return 0;
}
