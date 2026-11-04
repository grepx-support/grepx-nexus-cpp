/*
 * Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
 * This file is part of the GrepX platform and is proprietary and confidential.
 * Unauthorised copying, use, modification or distribution is strictly prohibited.
 */

#include "nexus/application/insert_service.hpp"
#include <cassert>
#include <iostream>
#include <vector>

using namespace nexus;

namespace {

struct FakeWriter {
    std::vector<domain::RowPtr> written;
    domain::Expected<void> upsert(const domain::TableName&, domain::RowPtr row) {
        written.push_back(std::move(row));
        return {};
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

void test_insert_writes_audits_and_publishes_in_order() {
    FakeWriter writer;
    FakeAuditor auditor;
    FakePublisher publisher;
    application::InsertService insert(writer, auditor, publisher);

    auto result = insert.execute(domain::TableName{"t"}, row_with("a"));

    assert(result.has_value());
    assert(writer.written.size() == 1);
    assert(auditor.append_calls == 1);
    assert(publisher.published.size() == 1);
    assert(publisher.published[0].kind == domain::ChangeKind::Inserted);
    std::cout << "test_insert_writes_audits_and_publishes_in_order: OK\n";
}

void test_insert_stops_on_writer_failure() {
    struct FailingWriter {
        domain::Expected<void> upsert(const domain::TableName&, domain::RowPtr) {
            return std::unexpected(domain::Error::TableNotFound);
        }
    };
    FailingWriter writer;
    FakeAuditor auditor;
    FakePublisher publisher;
    application::InsertService insert(writer, auditor, publisher);

    auto result = insert.execute(domain::TableName{"missing"}, row_with("a"));

    assert(!result.has_value());
    assert(auditor.append_calls == 0);   // must not audit a failed write
    assert(publisher.published.empty()); // must not publish a failed write
    std::cout << "test_insert_stops_on_writer_failure: OK\n";
}

} // namespace

int main() {
    test_insert_writes_audits_and_publishes_in_order();
    test_insert_stops_on_writer_failure();
    std::cout << "application/insert_service: all tests passed\n";
    return 0;
}
