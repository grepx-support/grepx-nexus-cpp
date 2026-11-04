/*
 * Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
 * This file is part of the GrepX platform and is proprietary and confidential.
 * Unauthorised copying, use, modification or distribution is strictly prohibited.
 */

#include "nexus/application/subscribe_service.hpp"
#include <cassert>
#include <iostream>

using namespace nexus;

namespace {

// Local fake parser — this is the whole point: SubscribeService links
// against zero lines of nexus_criteria. Any type satisfying
// ports::CriteriaParser works, proving the DIP boundary is real, not
// just documented.
struct FakeParser {
    bool should_fail = false;

    [[nodiscard]] domain::Expected<domain::PredicatePtr> parse(std::string_view) const {
        if (should_fail) return std::unexpected(domain::Error::ParseError);
        return domain::Predicate::always_true();
    }
};

struct FakeRegistry {
    std::vector<ports::Subscription> subs;
    void add(ports::Subscription s) { subs.push_back(std::move(s)); }
    void remove(const domain::TableName&, const std::string& id) {
        std::erase_if(subs, [&](const ports::Subscription& s) { return s.id == id; });
    }
    std::vector<ports::Subscription> for_table(const domain::TableName&) const { return subs; }
};

void test_subscribe_registers_on_successful_parse() {
    FakeRegistry registry;
    FakeParser parser;
    application::SubscribeService subscribe(registry, parser);

    auto result = subscribe.execute("sub-1", domain::TableName{"t"}, "amount > 100",
                                     [](const domain::ChangeEvent&) {});

    assert(result.has_value());
    assert(registry.subs.size() == 1);
    assert(registry.subs[0].id == "sub-1");
    std::cout << "test_subscribe_registers_on_successful_parse: OK\n";
}

void test_subscribe_propagates_parse_error_and_does_not_register() {
    FakeRegistry registry;
    FakeParser parser{.should_fail = true};
    application::SubscribeService subscribe(registry, parser);

    auto result = subscribe.execute("sub-1", domain::TableName{"t"}, "garbage",
                                     [](const domain::ChangeEvent&) {});

    assert(!result.has_value());
    assert(result.error() == domain::Error::ParseError);
    assert(registry.subs.empty());
    std::cout << "test_subscribe_propagates_parse_error_and_does_not_register: OK\n";
}

} // namespace

int main() {
    test_subscribe_registers_on_successful_parse();
    test_subscribe_propagates_parse_error_and_does_not_register();
    std::cout << "application/subscribe_service: all tests passed\n";
    return 0;
}
