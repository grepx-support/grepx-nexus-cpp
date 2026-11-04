/*
 * Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
 * This file is part of the GrepX platform and is proprietary and confidential.
 * Unauthorised copying, use, modification or distribution is strictly prohibited.
 */

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
