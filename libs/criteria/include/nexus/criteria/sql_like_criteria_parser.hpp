#pragma once
#include "nexus/criteria/parser.hpp"
#include "nexus/domain/error.hpp"
#include "nexus/domain/predicate.hpp"
#include <string_view>

namespace nexus::criteria {

// Adapter: satisfies ports::CriteriaParser. This is the only thing
// nexus_application should ever depend on from this library — the
// free function `parse()` above stays an implementation detail.
class SqlLikeCriteriaParser {
public:
    [[nodiscard]] domain::Expected<domain::PredicatePtr> parse(std::string_view criteria) const {
        return nexus::criteria::parse(criteria);
    }
};

} // namespace nexus::criteria
