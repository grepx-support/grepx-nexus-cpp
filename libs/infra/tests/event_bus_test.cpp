/*
 * Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
 * This file is part of the GrepX platform and is proprietary and confidential.
 * Unauthorised copying, use, modification or distribution is strictly prohibited.
 */

#include "nexus/infra/event_bus.hpp"
#include <cassert>
#include <iostream>

using namespace nexus;

namespace {

domain::RowPtr empty_row() {
    return std::make_shared<const domain::Row>(domain::RowKey{"k"},
                                                 std::unordered_map<std::string, domain::Value>{});
}

void test_publish_invokes_handler() {
    infra::EventBus bus;
    int calls = 0;
    bus.set_handler([&](const domain::ChangeEvent&) { ++calls; });

    bus.publish(domain::ChangeEvent{
        .table = domain::TableName{"t"}, .row = empty_row(), .kind = domain::ChangeKind::Inserted});
    assert(calls == 1);
    std::cout << "test_publish_invokes_handler: OK\n";
}

void test_publish_without_handler_does_not_crash() {
    infra::EventBus bus;
    bus.publish(domain::ChangeEvent{
        .table = domain::TableName{"t"}, .row = empty_row(), .kind = domain::ChangeKind::Inserted});
    std::cout << "test_publish_without_handler_does_not_crash: OK\n";
}

} // namespace

int main() {
    test_publish_invokes_handler();
    test_publish_without_handler_does_not_crash();
    std::cout << "infra/event_bus: all tests passed\n";
    return 0;
}
