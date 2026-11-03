/*
 * Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
 * This file is part of the GrepX platform and is proprietary and confidential.
 * Unauthorised copying, use, modification or distribution is strictly prohibited.
 */

// Ports are concepts, not runtime code — the meaningful test is that a
// minimal conforming type satisfies each concept, and a minimal
// non-conforming type does NOT. All checks are static_assert, evaluated
// at compile time; if this file compiles, the tests pass.
#include "nexus/ports/audit_writer.hpp"
#include "nexus/ports/criteria_parser.hpp"
#include "nexus/ports/event_publisher.hpp"
#include "nexus/ports/snapshot_reader.hpp"
#include "nexus/ports/snapshot_writer.hpp"
#include "nexus/ports/subscription_registry.hpp"
#include <iostream>

using namespace nexus;

namespace {

struct GoodReader {
    std::vector<domain::RowPtr> select(const domain::TableName&, const domain::PredicatePtr&) const {
        return {};
    }
};
static_assert(ports::SnapshotReader<GoodReader>);

// Missing select() entirely — must NOT satisfy the concept.
struct BadReader {};
static_assert(!ports::SnapshotReader<BadReader>);

struct GoodWriter {
    domain::Expected<void> upsert(const domain::TableName&, domain::RowPtr) { return {}; }
};
static_assert(ports::SnapshotWriter<GoodWriter>);

// A reader must NOT accidentally satisfy the writer concept (ISP is
// enforced by the type system, not just by convention).
static_assert(!ports::SnapshotWriter<GoodReader>);

struct GoodAuditor {
    void append(const domain::TableName&, domain::RowPtr) {}
};
static_assert(ports::AuditWriter<GoodAuditor>);

struct GoodPublisher {
    void publish(const domain::ChangeEvent&) {}
};
static_assert(ports::EventPublisher<GoodPublisher>);

struct GoodRegistry {
    void add(ports::Subscription) {}
    void remove(const domain::TableName&, const std::string&) {}
    std::vector<ports::Subscription> for_table(const domain::TableName&) const {
        return {};
    }
};
static_assert(ports::SubscriptionRegistry<GoodRegistry>);

struct GoodParser {
    domain::Expected<domain::PredicatePtr> parse(std::string_view) const {
        return domain::Predicate::always_true();
    }
};
static_assert(ports::CriteriaParser<GoodParser>);

} // namespace

int main() {
    std::cout << "ports: all static_assert concept checks passed\n";
    return 0;
}
