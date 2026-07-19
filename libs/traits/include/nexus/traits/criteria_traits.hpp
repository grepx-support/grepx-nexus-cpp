#pragma once
#include "nexus/criteria/sql_like_criteria_parser.hpp"
#include "nexus/ports/criteria_parser.hpp"

namespace nexus::traits {

struct CriteriaTraits {
    using Parser = criteria::SqlLikeCriteriaParser;
};

template <typename T>
concept CriteriaTraitsConcept = requires {
    typename T::Parser;
} && ports::CriteriaParser<typename T::Parser>;

static_assert(CriteriaTraitsConcept<CriteriaTraits>);

} // namespace nexus::traits
