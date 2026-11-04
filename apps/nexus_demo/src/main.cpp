/*
 * Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
 * This file is part of the GrepX platform and is proprietary and confidential.
 * Unauthorised copying, use, modification or distribution is strictly prohibited.
 */

#include "nexus/app/app_launcher.hpp"
#include "nexus/traits/production_traits.hpp"
#include <iostream>
#include <unordered_map>

using namespace nexus;

namespace {

domain::RowPtr make_row(std::string key, std::int64_t amount, std::string currency) {
    return std::make_shared<const domain::Row>(
        domain::RowKey{std::move(key)},
        std::unordered_map<std::string, domain::Value>{
            {"amount", domain::Value{amount}},
            {"currency", domain::Value{std::move(currency)}},
        });
}

void print_row(const domain::RowPtr& row) {
    const auto* amount = row->find("amount");
    const auto* currency = row->find("currency");
    std::cout << "  " << row->key().value() << " amount="
              << (amount ? std::get<std::int64_t>(*amount) : 0) << " currency="
              << (currency ? std::get<std::string>(*currency) : "?") << '\n';
}

// The demo "runner" — this is the ONLY thing specific to nexus_demo.
// Everything about wiring the traits into a working engine is shared
// with nexus_server via nexus::app::AppEngine<T>.
template <typename Engine>
int run_console_demo(Engine& engine, int /*argc*/, char* /*argv*/[]) {
    const domain::TableName transactions{"transactions"};

    engine.subscribe().execute("client-large", transactions, "amount > 100",
                                [](const domain::ChangeEvent& event) {
                                    std::cout << "[client-large] notified: ";
                                    print_row(event.row);
                                });
    engine.subscribe().execute("client-usd", transactions, "currency = 'USD'",
                                [](const domain::ChangeEvent& event) {
                                    std::cout << "[client-usd] notified: ";
                                    print_row(event.row);
                                });

    std::cout << "--- inserting rows ---\n";
    engine.insert().execute(transactions, make_row("tx-1", 50, "USD"));
    engine.insert().execute(transactions, make_row("tx-2", 150, "EUR"));
    engine.insert().execute(transactions, make_row("tx-3", 200, "USD"));

    std::cout << "--- select amount > 100 ---\n";
    auto predicate = engine.criteria_parser().parse("amount > 100");
    if (!predicate) {
        std::cerr << "parse failed\n";
        return 1;
    }
    for (const auto& row : engine.select().execute(transactions, *predicate)) {
        print_row(row);
    }

    std::cout << "audit entries for transactions: " << engine.audit_log().count(transactions) << '\n';
    return 0;
}

} // namespace

int main(int argc, char* argv[]) {
    using SnapshotTraits = nexus::traits::SnapshotTraits;
    using AuditTraits = nexus::traits::AuditTraits;
    using EventTraits = nexus::traits::EventTraits;
    using SubscriptionTraits = nexus::traits::SubscriptionTraits;
    using CriteriaTraits = nexus::traits::CriteriaTraits;
    using ProductionTraits =
        nexus::traits::ProductionTraits<SnapshotTraits, AuditTraits, EventTraits,
                                         SubscriptionTraits, CriteriaTraits>;

    return nexus::app::launch_app<ProductionTraits>(
        argc, argv,
        [](auto& engine, int a, char* v[]) { return run_console_demo(engine, a, v); });
}
