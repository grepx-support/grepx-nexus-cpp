/*
 * Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
 * This file is part of the GrepX platform and is proprietary and confidential.
 * Unauthorised copying, use, modification or distribution is strictly prohibited.
 */

#include "nexus/infra/subscription_registry.hpp"
#include <atomic>
#include <cassert>
#include <iostream>
#include <thread>

using namespace nexus;

namespace {

// Regression test for a real data race caught under ThreadSanitizer:
// for_table() used to return a reference into the internal vector after
// releasing the lock. A concurrent add() reallocating that vector while
// another thread iterated it was a genuine race (verified reproducible,
// not hypothetical). for_table() now copies under the lock. This test
// hammers both operations concurrently — it won't prove absence of
// races by itself, but combined with a ThreadSanitizer CI build it's
// the regression guard against this exact bug coming back.
void test_concurrent_add_and_for_table_does_not_race() {
    infra::SubscriptionRegistry registry;
    const domain::TableName table{"t"};

    std::thread adder([&] {
        for (int i = 0; i < 2000; ++i) {
            registry.add(ports::Subscription{
                .id = "s" + std::to_string(i), .table = table,
                .predicate = domain::Predicate::always_true(),
                .notify = [](const domain::ChangeEvent&) {}});
        }
    });

    std::thread reader([&] {
        for (int i = 0; i < 2000; ++i) {
            for (const auto& sub : registry.for_table(table)) {
                assert(!sub.id.empty()); // touch every returned element
            }
        }
    });

    adder.join();
    reader.join();
    assert(registry.for_table(table).size() == 2000);
    std::cout << "test_concurrent_add_and_for_table_does_not_race: OK\n";
}

} // namespace

int main() {
    test_concurrent_add_and_for_table_does_not_race();
    std::cout << "infra/subscription_registry_stress: all tests passed\n";
    return 0;
}
