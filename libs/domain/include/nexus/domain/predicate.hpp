#pragma once
#include "nexus/domain/row.hpp"
#include "nexus/domain/value.hpp"
#include <memory>
#include <string>
#include <variant>

namespace nexus::domain {

enum class CompareOp { Eq, Ne, Gt, Ge, Lt, Le };

struct Predicate; // fwd decl, nodes hold children via shared_ptr (recursive variant)
using PredicatePtr = std::shared_ptr<const Predicate>;

struct AlwaysTrue {};

struct Comparison {
    std::string field;
    CompareOp op;
    Value value;
};

struct And {
    PredicatePtr lhs;
    PredicatePtr rhs;
};

struct Or {
    PredicatePtr lhs;
    PredicatePtr rhs;
};

struct Predicate {
    std::variant<AlwaysTrue, Comparison, And, Or> node;

    [[nodiscard]] static PredicatePtr always_true() {
        static const PredicatePtr instance = std::make_shared<const Predicate>(
            Predicate{.node = AlwaysTrue{}});
        return instance;
    }
};

namespace detail {

// Ordering is only well-defined between same-typed, non-monostate values.
// Type or NULL mismatches simply never satisfy a comparison.
inline bool compare(const Value& lhs, CompareOp op, const Value& rhs) {
    if (lhs.index() != rhs.index() || lhs.index() == 0) return false;

    return std::visit(
        [&]<typename L>(const L& l) -> bool {
            const auto& r = std::get<L>(rhs);
            if constexpr (std::is_same_v<L, std::monostate>) {
                return false;
            } else {
                switch (op) {
                    case CompareOp::Eq: return l == r;
                    case CompareOp::Ne: return l != r;
                    case CompareOp::Gt: return l > r;
                    case CompareOp::Ge: return l >= r;
                    case CompareOp::Lt: return l < r;
                    case CompareOp::Le: return l <= r;
                }
                return false;
            }
        },
        lhs);
}

} // namespace detail

[[nodiscard]] inline bool matches(const Predicate& predicate, const Row& row) {
    return std::visit(
        [&]<typename Node>(const Node& node) -> bool {
            if constexpr (std::is_same_v<Node, AlwaysTrue>) {
                return true;
            } else if constexpr (std::is_same_v<Node, Comparison>) {
                const Value* field = row.find(node.field);
                if (!field) return false;
                return detail::compare(*field, node.op, node.value);
            } else if constexpr (std::is_same_v<Node, And>) {
                return matches(*node.lhs, row) && matches(*node.rhs, row);
            } else if constexpr (std::is_same_v<Node, Or>) {
                return matches(*node.lhs, row) || matches(*node.rhs, row);
            }
        },
        predicate.node);
}

[[nodiscard]] inline bool matches(const PredicatePtr& predicate, const Row& row) {
    return matches(*predicate, row);
}

} // namespace nexus::domain
