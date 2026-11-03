/*
 * Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
 * This file is part of the GrepX platform and is proprietary and confidential.
 * Unauthorised copying, use, modification or distribution is strictly prohibited.
 */

#pragma once
#include "nexus/domain/error.hpp"
#include "nexus/ports/criteria_parser.hpp"
#include "nexus/ports/subscription_registry.hpp"
#include <functional>
#include <string>
#include <string_view>

namespace nexus::application {

// Parses the SQL-like criteria string once, at subscribe time (cold path),
// and registers a compiled Predicate — evaluation later is pure C++, no
// re-parsing on the hot insert/notify path.
//
// The parser is injected (DIP) — this class depends on ports::CriteriaParser
// only, never on nexus_criteria's concrete implementation. Swap grammars
// or move parsing to an out-of-process control-plane service without
// touching this file.
template <ports::SubscriptionRegistry Registry, ports::CriteriaParser Parser>
class SubscribeService {
public:
    SubscribeService(Registry& registry, const Parser& parser)
        : registry_(registry), parser_(parser) {}

    domain::Expected<void>
    execute(std::string subscription_id, const domain::TableName& table,
            std::string_view criteria,
            std::function<void(const domain::ChangeEvent&)> notify) {
        auto predicate = parser_.parse(criteria);
        if (!predicate) return std::unexpected(predicate.error());

        registry_.add(ports::Subscription{
            .id = std::move(subscription_id),
            .table = table,
            .predicate = *predicate,
            .notify = std::move(notify),
        });
        return {};
    }

private:
    Registry& registry_;
    const Parser& parser_;
};

} // namespace nexus::application
