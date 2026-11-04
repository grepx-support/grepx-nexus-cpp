/*
 * Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
 * This file is part of the GrepX platform and is proprietary and confidential.
 * Unauthorised copying, use, modification or distribution is strictly prohibited.
 */

#include "nexus/criteria/sql_like_criteria_parser.hpp"
#include "nexus/ports/criteria_parser.hpp"
#include <cassert>
#include <iostream>

using namespace nexus;

// Compile-time proof that the adapter satisfies the port this library
// is supposed to fulfil for nexus_application.
static_assert(ports::CriteriaParser<criteria::SqlLikeCriteriaParser>);

namespace {

domain::RowPtr row_with(std::int64_t amount, std::string currency) {
    return std::make_shared<const domain::Row>(
        domain::RowKey{"k"},
        std::unordered_map<std::string, domain::Value>{
            {"amount", domain::Value{amount}},
            {"currency", domain::Value{std::move(currency)}},
        });
}

void test_empty_criteria_matches_all() {
    auto predicate = criteria::parse("");
    assert(predicate.has_value());
    assert(domain::matches(*predicate, *row_with(0, "")));
    std::cout << "test_empty_criteria_matches_all: OK\n";
}

void test_comparison_operators() {
    assert(domain::matches(*criteria::parse("amount > 100"), *row_with(150, "USD")));
    assert(domain::matches(*criteria::parse("amount >= 150"), *row_with(150, "USD")));
    assert(domain::matches(*criteria::parse("amount < 200"), *row_with(150, "USD")));
    assert(domain::matches(*criteria::parse("amount != 100"), *row_with(150, "USD")));
    assert(domain::matches(*criteria::parse("currency = 'USD'"), *row_with(150, "USD")));
    std::cout << "test_comparison_operators: OK\n";
}

void test_and_or_precedence() {
    // AND binds tighter than OR: "a OR b AND c" == "a OR (b AND c)"
    auto predicate = criteria::parse("amount > 1000 OR amount > 100 AND currency = 'USD'");
    assert(domain::matches(*predicate, *row_with(150, "USD")));
    assert(!domain::matches(*predicate, *row_with(150, "EUR")));
    std::cout << "test_and_or_precedence: OK\n";
}

void test_malformed_input_rejected() {
    auto predicate = criteria::parse("amount >");
    assert(!predicate.has_value());
    assert(predicate.error() == domain::Error::ParseError);
    std::cout << "test_malformed_input_rejected: OK\n";
}

void test_adapter_matches_free_function() {
    criteria::SqlLikeCriteriaParser parser;
    auto a = parser.parse("amount > 100");
    auto b = criteria::parse("amount > 100");
    assert(a.has_value() && b.has_value());
    assert(domain::matches(*a, *row_with(150, "USD")) == domain::matches(*b, *row_with(150, "USD")));
    std::cout << "test_adapter_matches_free_function: OK\n";
}

} // namespace

int main() {
    test_empty_criteria_matches_all();
    test_comparison_operators();
    test_and_or_precedence();
    test_malformed_input_rejected();
    test_adapter_matches_free_function();
    std::cout << "criteria: all tests passed\n";
    return 0;
}
