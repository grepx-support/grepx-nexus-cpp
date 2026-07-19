#include "nexus/application/transition_service.hpp"
#include "nexus/statemachine/transaction_transitions.hpp"
#include <cassert>
#include <iostream>
#include <vector>

using namespace nexus;

namespace {

struct FakeConditionalWriter {
    std::uint64_t version_to_return = 0;
    bool fail_with_conflict = false;
    int calls = 0;

    domain::Expected<std::uint64_t>
    compare_and_swap(const domain::TableName&, const domain::RowKey&, std::uint64_t, domain::RowPtr) {
        ++calls;
        if (fail_with_conflict) return std::unexpected(domain::Error::VersionConflict);
        return version_to_return;
    }
};

struct FakeAuditor {
    int append_calls = 0;
    void append(const domain::TableName&, domain::RowPtr) { ++append_calls; }
};

struct FakePublisher {
    std::vector<domain::ChangeEvent> published;
    void publish(const domain::ChangeEvent& event) { published.push_back(event); }
};

domain::RowPtr row_with(std::string key) {
    return std::make_shared<const domain::Row>(domain::RowKey{std::move(key)},
                                                 std::unordered_map<std::string, domain::Value>{});
}

void test_allowed_transition_writes_audits_and_publishes() {
    FakeConditionalWriter writer{.version_to_return = 2};
    FakeAuditor auditor;
    FakePublisher publisher;
    application::TransitionService transition(writer, auditor, publisher,
                                               statemachine::transition_state_machine());

    auto result = transition.execute(domain::TableName{"transactions"}, domain::RowKey{"tx-1"},
                                      "NEW", "CLAIMED", /*expected_version=*/1, row_with("tx-1"));

    assert(result.has_value());
    assert(*result == 2);
    assert(writer.calls == 1);
    assert(auditor.append_calls == 1);
    assert(publisher.published.size() == 1);
    assert(publisher.published[0].kind == domain::ChangeKind::Updated);
    std::cout << "test_allowed_transition_writes_audits_and_publishes: OK\n";
}

void test_illegal_transition_never_touches_writer() {
    FakeConditionalWriter writer;
    FakeAuditor auditor;
    FakePublisher publisher;
    application::TransitionService transition(writer, auditor, publisher,
                                               statemachine::transition_state_machine());

    auto result = transition.execute(domain::TableName{"transactions"}, domain::RowKey{"tx-1"},
                                      "NEW", "COMPLETED", /*expected_version=*/1, row_with("tx-1"));

    assert(!result.has_value());
    assert(result.error() == domain::Error::InvalidTransition);
    assert(writer.calls == 0);
    assert(auditor.append_calls == 0);
    assert(publisher.published.empty());
    std::cout << "test_illegal_transition_never_touches_writer: OK\n";
}

void test_version_conflict_stops_before_audit_or_publish() {
    FakeConditionalWriter writer{.fail_with_conflict = true};
    FakeAuditor auditor;
    FakePublisher publisher;
    application::TransitionService transition(writer, auditor, publisher,
                                               statemachine::transition_state_machine());

    auto result = transition.execute(domain::TableName{"transactions"}, domain::RowKey{"tx-1"},
                                      "NEW", "CLAIMED", /*expected_version=*/1, row_with("tx-1"));

    assert(!result.has_value());
    assert(result.error() == domain::Error::VersionConflict);
    assert(auditor.append_calls == 0);
    assert(publisher.published.empty());
    std::cout << "test_version_conflict_stops_before_audit_or_publish: OK\n";
}

} // namespace

int main() {
    test_allowed_transition_writes_audits_and_publishes();
    test_illegal_transition_never_touches_writer();
    test_version_conflict_stops_before_audit_or_publish();
    std::cout << "application/transition_service: all tests passed\n";
    return 0;
}
