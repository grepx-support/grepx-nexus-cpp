#include "nexus/infra/audit_log.hpp"
#include <cassert>
#include <iostream>

using namespace nexus;

namespace {

domain::RowPtr row_with(std::string key) {
    return std::make_shared<const domain::Row>(domain::RowKey{std::move(key)},
                                                 std::unordered_map<std::string, domain::Value>{});
}

void test_append_increments_count() {
    infra::AuditLog log;
    const domain::TableName table{"t"};
    assert(log.count(table) == 0);
    log.append(table, row_with("a"));
    log.append(table, row_with("b"));
    assert(log.count(table) == 2);
    std::cout << "test_append_increments_count: OK\n";
}

void test_tables_are_independent() {
    infra::AuditLog log;
    log.append(domain::TableName{"t1"}, row_with("a"));
    assert(log.count(domain::TableName{"t1"}) == 1);
    assert(log.count(domain::TableName{"t2"}) == 0);
    std::cout << "test_tables_are_independent: OK\n";
}

} // namespace

int main() {
    test_append_increments_count();
    test_tables_are_independent();
    std::cout << "infra/audit_log: all tests passed\n";
    return 0;
}
