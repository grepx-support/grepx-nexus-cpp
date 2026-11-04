/*
 * Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
 * This file is part of the GrepX platform and is proprietary and confidential.
 * Unauthorised copying, use, modification or distribution is strictly prohibited.
 */

#include "nexus/persistence/sqlite_snapshot_store.hpp"
#include <cassert>
#include <cstdio>
#include <iostream>

using namespace nexus;

namespace {

domain::RowPtr row_with(std::string key, std::int64_t amount, std::string currency) {
    return std::make_shared<const domain::Row>(
        domain::RowKey{std::move(key)},
        std::unordered_map<std::string, domain::Value>{
            {"amount", domain::Value{amount}},
            {"currency", domain::Value{std::move(currency)}},
        });
}

void test_upsert_then_select_matching() {
    std::remove("test_basic.db");
    persistence::SqliteSnapshotStore store("test_basic.db");
    const domain::TableName table{"t"};

    store.upsert(table, row_with("a", 50, "EUR"));
    store.upsert(table, row_with("b", 150, "USD"));

    auto pred = std::make_shared<const domain::Predicate>(
        domain::Predicate{.node = domain::Comparison{"amount", domain::CompareOp::Gt,
                                                       domain::Value{std::int64_t{100}}}});
    auto results = store.select(table, pred);
    assert(results.size() == 1);
    assert(results[0]->key() == domain::RowKey{"b"});
    std::remove("test_basic.db");
    std::cout << "test_upsert_then_select_matching: OK\n";
}

void test_upsert_same_key_replaces() {
    std::remove("test_replace.db");
    persistence::SqliteSnapshotStore store("test_replace.db");
    const domain::TableName table{"t"};

    store.upsert(table, row_with("a", 50, "EUR"));
    store.upsert(table, row_with("a", 999, "GBP"));

    auto results = store.select(table, domain::Predicate::always_true());
    assert(results.size() == 1);
    assert(std::get<std::int64_t>(*results[0]->find("amount")) == 999);
    assert(std::get<std::string>(*results[0]->find("currency")) == "GBP");
    std::remove("test_replace.db");
    std::cout << "test_upsert_same_key_replaces: OK\n";
}

// THE actual persistence proof: open a store, write, close it (process
// A). Open a brand-new store instance pointed at the same file (process
// B, simulated by a fresh object here) and confirm the data is still
// there. This is what "persisted" means, not just "worked while the
// process was alive."
void test_data_survives_store_destruction_and_reopen() {
    const std::string path = "test_persistence.db";
    std::remove(path.c_str());

    {
        persistence::SqliteSnapshotStore store(path);
        store.upsert(domain::TableName{"transactions"}, row_with("tx-1", 200, "USD"));
    } // store destructs here — connection closed, exactly like a process exiting

    {
        persistence::SqliteSnapshotStore reopened(path); // fresh instance, same file
        auto results = reopened.select(domain::TableName{"transactions"},
                                        domain::Predicate::always_true());
        assert(results.size() == 1);
        assert(results[0]->key() == domain::RowKey{"tx-1"});
        assert(std::get<std::int64_t>(*results[0]->find("amount")) == 200);
    }

    std::remove(path.c_str());
    std::cout << "test_data_survives_store_destruction_and_reopen: OK\n";
}

} // namespace

int main() {
    test_upsert_then_select_matching();
    test_upsert_same_key_replaces();
    test_data_survives_store_destruction_and_reopen();
    std::cout << "persistence/sqlite_snapshot_store: all tests passed\n";
    return 0;
}
