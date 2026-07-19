#include "nexus/domain/predicate.hpp"
#include <cassert>
#include <iostream>

using namespace nexus::domain;

namespace {

RowPtr row_with(std::int64_t amount, std::string currency) {
    return std::make_shared<const Row>(
        RowKey{"k"},
        std::unordered_map<std::string, Value>{
            {"amount", Value{amount}},
            {"currency", Value{std::move(currency)}},
        });
}

void test_always_true_matches_everything() {
    assert(matches(Predicate::always_true(), *row_with(0, "")));
    std::cout << "test_always_true_matches_everything: OK\n";
}

void test_comparison_gt() {
    Predicate p{.node = Comparison{"amount", CompareOp::Gt, Value{std::int64_t{100}}}};
    assert(matches(p, *row_with(150, "USD")));
    assert(!matches(p, *row_with(50, "USD")));
    std::cout << "test_comparison_gt: OK\n";
}

void test_and_combinator() {
    auto lhs = std::make_shared<const Predicate>(
        Predicate{.node = Comparison{"amount", CompareOp::Gt, Value{std::int64_t{100}}}});
    auto rhs = std::make_shared<const Predicate>(
        Predicate{.node = Comparison{"currency", CompareOp::Eq, Value{std::string{"USD"}}}});
    Predicate both{.node = And{lhs, rhs}};

    assert(matches(both, *row_with(150, "USD")));
    assert(!matches(both, *row_with(150, "EUR")));
    std::cout << "test_and_combinator: OK\n";
}

void test_type_mismatch_never_matches() {
    // Comparing a string field against a numeric literal must not match —
    // it's a type mismatch, not a coercion opportunity.
    Predicate p{.node = Comparison{"currency", CompareOp::Eq, Value{std::int64_t{5}}}};
    assert(!matches(p, *row_with(0, "5")));
    std::cout << "test_type_mismatch_never_matches: OK\n";
}

} // namespace

int main() {
    test_always_true_matches_everything();
    test_comparison_gt();
    test_and_combinator();
    test_type_mismatch_never_matches();
    std::cout << "domain/predicate: all tests passed\n";
    return 0;
}
