/*
 * Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
 * This file is part of the GrepX platform and is proprietary and confidential.
 * Unauthorised copying, use, modification or distribution is strictly prohibited.
 */

#pragma once
#include "nexus/domain/error.hpp"
#include "nexus/domain/predicate.hpp"
#include <string_view>

namespace nexus::ports {

// Application depends on this concept, never on nexus_criteria directly.
// SubscribeService is injected a parser satisfying this — swap in a
// different implementation (a different grammar, a Python microservice
// proxy, whatever) without application changing at all (DIP).
template <typename T>
concept CriteriaParser = requires(const T& parser, std::string_view criteria) {
    { parser.parse(criteria) } -> std::same_as<domain::Expected<domain::PredicatePtr>>;
};

} // namespace nexus::ports
