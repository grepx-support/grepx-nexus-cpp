#include "nexus/infra/snapshot_store.hpp"
#include <cassert>
#include <iostream>

using namespace nexus;

namespace {

domain::RowPtr row_with(std::string key, std::int64_t amount) {
    return std::make_shared<const domain::Row>(
        domain::RowKey{std::move(key)},
        std::unordered_map<std::string, domain::Value>{{"amount", domain::Value{amount}}});
}

void test_upsert_then_select_matching() {
    infra::SnapshotStore store;
    const domain::TableName table{"t"};
    store.upsert(table, row_with("a", 50));
    store.upsert(table, row_with("b", 150));

    auto pred = std::make_shared<const domain::Predicate>(
        domain::Predicate{.node = domain::Comparison{"amount", domain::CompareOp::Gt,
                                                       domain::Value{std::int64_t{100}}}});
    auto results = store.select(table, pred);
    assert(results.size() == 1);
    assert(results[0]->key() == domain::RowKey{"b"});
    std::cout << "test_upsert_then_select_matching: OK\n";
}

void test_select_unknown_table_returns_empty() {
    infra::SnapshotStore store;
    auto results = store.select(domain::TableName{"missing"}, domain::Predicate::always_true());
    assert(results.empty());
    std::cout << "test_select_unknown_table_returns_empty: OK\n";
}

void test_upsert_same_key_replaces() {
    infra::SnapshotStore store;
    const domain::TableName table{"t"};
    store.upsert(table, row_with("a", 50));
    store.upsert(table, row_with("a", 999));

    auto results = store.select(table, domain::Predicate::always_true());
    assert(results.size() == 1);
    assert(std::get<std::int64_t>(*results[0]->find("amount")) == 999);
    std::cout << "test_upsert_same_key_replaces: OK\n";
}

} // namespace

int main() {
    test_upsert_then_select_matching();
    test_select_unknown_table_returns_empty();
    test_upsert_same_key_replaces();
    std::cout << "infra/snapshot_store: all tests passed\n";
    return 0;
}
