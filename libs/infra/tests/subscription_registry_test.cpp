#include "nexus/infra/subscription_registry.hpp"
#include <cassert>
#include <iostream>

using namespace nexus;

namespace {

void test_add_and_for_table() {
    infra::SubscriptionRegistry registry;
    const domain::TableName table{"t"};
    registry.add(ports::Subscription{
        .id = "sub-1", .table = table, .predicate = domain::Predicate::always_true(), .notify = [](const domain::ChangeEvent&) {}});

    assert(registry.for_table(table).size() == 1);
    assert(registry.for_table(domain::TableName{"other"}).empty());
    std::cout << "test_add_and_for_table: OK\n";
}

void test_remove_by_id() {
    infra::SubscriptionRegistry registry;
    const domain::TableName table{"t"};
    registry.add(ports::Subscription{
        .id = "sub-1", .table = table, .predicate = domain::Predicate::always_true(), .notify = [](const domain::ChangeEvent&) {}});
    registry.add(ports::Subscription{
        .id = "sub-2", .table = table, .predicate = domain::Predicate::always_true(), .notify = [](const domain::ChangeEvent&) {}});

    registry.remove(table, "sub-1");
    const auto& remaining = registry.for_table(table);
    assert(remaining.size() == 1);
    assert(remaining[0].id == "sub-2");
    std::cout << "test_remove_by_id: OK\n";
}

} // namespace

int main() {
    test_add_and_for_table();
    test_remove_by_id();
    std::cout << "infra/subscription_registry: all tests passed\n";
    return 0;
}
