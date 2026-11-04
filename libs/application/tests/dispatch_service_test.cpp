/*
 * Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
 * This file is part of the GrepX platform and is proprietary and confidential.
 * Unauthorised copying, use, modification or distribution is strictly prohibited.
 */

#include "nexus/application/dispatch_service.hpp"
#include <cassert>
#include <iostream>

using namespace nexus;

namespace {

struct FakeRegistry {
    std::vector<ports::Subscription> subs;
    void add(ports::Subscription s) { subs.push_back(std::move(s)); }
    void remove(const domain::TableName&, const std::string&) {}
    std::vector<ports::Subscription> for_table(const domain::TableName&) const { return subs; }
};

domain::RowPtr row_with(std::int64_t amount) {
    return std::make_shared<const domain::Row>(
        domain::RowKey{"k"}, std::unordered_map<std::string, domain::Value>{{"amount", domain::Value{amount}}});
}

void test_dispatch_notifies_only_matching_subscribers() {
    FakeRegistry registry;
    int matched_calls = 0;
    int unmatched_calls = 0;

    auto gt100 = std::make_shared<const domain::Predicate>(
        domain::Predicate{.node = domain::Comparison{"amount", domain::CompareOp::Gt,
                                                       domain::Value{std::int64_t{100}}}});
    auto gt1000 = std::make_shared<const domain::Predicate>(
        domain::Predicate{.node = domain::Comparison{"amount", domain::CompareOp::Gt,
                                                       domain::Value{std::int64_t{1000}}}});

    registry.subs.push_back(ports::Subscription{
        .id = "matches", .table = domain::TableName{"t"}, .predicate = gt100,
        .notify = [&](const domain::ChangeEvent&) { ++matched_calls; }});
    registry.subs.push_back(ports::Subscription{
        .id = "no-match", .table = domain::TableName{"t"}, .predicate = gt1000,
        .notify = [&](const domain::ChangeEvent&) { ++unmatched_calls; }});

    application::DispatchService dispatch(registry);
    dispatch.on_event(domain::ChangeEvent{
        .table = domain::TableName{"t"}, .row = row_with(150), .kind = domain::ChangeKind::Inserted});

    assert(matched_calls == 1);
    assert(unmatched_calls == 0);
    std::cout << "test_dispatch_notifies_only_matching_subscribers: OK\n";
}

} // namespace

int main() {
    test_dispatch_notifies_only_matching_subscribers();
    std::cout << "application/dispatch_service: all tests passed\n";
    return 0;
}
