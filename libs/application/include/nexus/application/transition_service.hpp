/*
 * Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
 * This file is part of the GrepX platform and is proprietary and confidential.
 * Unauthorised copying, use, modification or distribution is strictly prohibited.
 */

#pragma once
#include "nexus/domain/change_event.hpp"
#include "nexus/domain/error.hpp"
#include "nexus/ports/audit_writer.hpp"
#include "nexus/ports/conditional_writer.hpp"
#include "nexus/ports/event_publisher.hpp"
#include "nexus/statemachine/state_machine.hpp"
#include <cstdint>
#include <string_view>

namespace nexus::application {

// Moves a row through its lifecycle: validates from->to against an
// injected StateMachine, then applies the update only if the row's
// version still matches what the caller last read (compare-and-swap).
// A lost race or an illegal transition both fail closed — no partial
// write, no audit entry, no published event.
template <ports::ConditionalWriter Writer, ports::AuditWriter Auditor,
          ports::EventPublisher Publisher>
class TransitionService {
public:
    TransitionService(Writer& writer, Auditor& auditor, Publisher& publisher,
                       statemachine::StateMachine machine) noexcept
        : writer_(writer), auditor_(auditor), publisher_(publisher), machine_(machine) {}

    domain::Expected<std::uint64_t>
    execute(const domain::TableName& table, const domain::RowKey& key,
            std::string_view from, std::string_view to,
            std::uint64_t expected_version, domain::RowPtr new_row) {
        if (!machine_.is_allowed(from, to)) {
            return std::unexpected(domain::Error::InvalidTransition);
        }

        auto version = writer_.compare_and_swap(table, key, expected_version, new_row);
        if (!version) {
            return std::unexpected(version.error());
        }

        auditor_.append(table, new_row);
        publisher_.publish(domain::ChangeEvent{
            .table = table,
            .row = new_row,
            .kind = domain::ChangeKind::Updated,
        });
        return *version;
    }

private:
    Writer& writer_;
    Auditor& auditor_;
    Publisher& publisher_;
    statemachine::StateMachine machine_;
};

} // namespace nexus::application
