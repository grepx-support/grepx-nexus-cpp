#pragma once
#include "nexus/domain/error.hpp"
#include "nexus/domain/row.hpp"
#include "nexus/domain/row_key.hpp"
#include "nexus/domain/table_name.hpp"
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace nexus::ports {

// Result of one submit() — enough for SubmitService to know what
// happened without a second round trip: whether this was the row's
// first write (created), what version it's at now, what its status
// was immediately before (empty if created), and which outbox row
// recorded the attempt (so it can be marked published after a
// successful publish).
struct SubmitResult {
    std::uint64_t version;
    std::string from_status; // empty when created == true
    bool created;
    std::uint64_t outbox_id;
};

// One business transaction, submitted once, applied atomically.
//
// Replaces the old two-round-trip client pattern (an insert() into a
// hand-named "stage" table, then a separate insert()/transition() into
// the snapshot table) with a single call that does everything inside
// one SQLite transaction: read the row's current status (if any),
// validate the requested transition via `is_allowed`, upsert/CAS the
// snapshot, and durably record the attempt in the outbox table. A
// client that crashes mid-call sees either "nothing happened" or
// "everything happened" — never a stage-table row with no matching
// snapshot update, which was possible before this existed.
//
// `is_allowed` is a callback rather than a hard dependency on
// nexus::statemachine::StateMachine — persistence shouldn't need to
// know the state machine exists, only that something can answer
// "is this transition legal". When no row exists yet (first submit
// for this key), the writer treats it as always allowed and never
// calls is_allowed at all — there's no prior status to validate a
// transition from.
template <typename T>
concept SubmitWriter = requires(T& writer, const domain::TableName& table,
                                 const domain::RowKey& key, domain::RowPtr row,
                                 const std::function<bool(std::string_view, std::string_view)>& is_allowed,
                                 std::uint64_t outbox_id) {
    { writer.submit(table, key, row, is_allowed) } -> std::same_as<domain::Expected<SubmitResult>>;
    { writer.mark_published(outbox_id) } -> std::same_as<void>;
};

} // namespace nexus::ports
