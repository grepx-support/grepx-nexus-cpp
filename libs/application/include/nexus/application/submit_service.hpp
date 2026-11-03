/*
 * Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
 * This file is part of the GrepX platform and is proprietary and confidential.
 * Unauthorised copying, use, modification or distribution is strictly prohibited.
 */

#pragma once
#include "nexus/domain/change_event.hpp"
#include "nexus/domain/error.hpp"
#include "nexus/ports/event_publisher.hpp"
#include "nexus/ports/submit_writer.hpp"
#include "nexus/statemachine/state_machine.hpp"

namespace nexus::application {

// The whole point of this service: a client sends ONE request —
// "here's the new state of transaction X" — and everything else
// (deciding whether that's a legal move from wherever X currently is,
// writing the durable audit/outbox record, updating the snapshot, and
// telling subscribers) happens here, server-side, as one atomic unit.
// See ports::SubmitWriter for why that atomicity is real and not just
// "these calls happen to run back to back".
//
// Callers no longer supply from_status or expected_version — the
// server already knows both (it just read them inside writer_.submit()
// under the same lock/transaction that applies the write), so there's
// nothing for the client to get stale or race on.
template <ports::SubmitWriter Writer, ports::EventPublisher Publisher>
class SubmitService {
public:
    SubmitService(Writer& writer, Publisher& publisher, statemachine::StateMachine machine) noexcept
        : writer_(writer), publisher_(publisher), machine_(machine) {}

    domain::Expected<ports::SubmitResult>
    execute(const domain::TableName& table, const domain::RowKey& key, domain::RowPtr row) {
        auto is_allowed = [this](std::string_view from, std::string_view to) {
            return machine_.is_allowed(from, to);
        };

        auto result = writer_.submit(table, key, row, is_allowed);
        if (!result) return result;

        publisher_.publish(domain::ChangeEvent{
            .table = table,
            .row = row,
            .kind = result->created ? domain::ChangeKind::Inserted : domain::ChangeKind::Updated,
        });

        // Publish is synchronous and in-process here, so "delivered"
        // effectively means "the call above returned" — but this is
        // still a separate step from the write on purpose (see
        // SubmitWriter::mark_published), not folded into submit()
        // itself, so the durability guarantee doesn't quietly start
        // depending on the event bus never throwing.
        writer_.mark_published(result->outbox_id);

        return result;
    }

private:
    Writer& writer_;
    Publisher& publisher_;
    statemachine::StateMachine machine_;
};

} // namespace nexus::application
