#include "nexus/protocol/codec.hpp"
#include <cassert>
#include <iostream>

using namespace nexus;

namespace {

void test_select_round_trip() {
    protocol::SelectRequest req{domain::TableName{"transactions"}, "amount > 100"};
    std::string line = protocol::encode(req);
    auto parsed = protocol::parse_request(line);

    assert(parsed.has_value());
    auto* select = std::get_if<protocol::SelectRequest>(&*parsed);
    assert(select != nullptr);
    assert(select->table.value() == "transactions");
    assert(select->criteria == "amount > 100");
    std::cout << "test_select_round_trip: OK\n";
}

void test_select_empty_criteria() {
    protocol::SelectRequest req{domain::TableName{"t"}, ""};
    auto parsed = protocol::parse_request(protocol::encode(req));
    assert(parsed.has_value());
    auto* select = std::get_if<protocol::SelectRequest>(&*parsed);
    assert(select != nullptr);
    assert(select->criteria.empty());
    std::cout << "test_select_empty_criteria: OK\n";
}

void test_insert_round_trip_preserves_types() {
    auto row = std::make_shared<const domain::Row>(
        domain::RowKey{"tx-1"},
        std::unordered_map<std::string, domain::Value>{
            {"amount", domain::Value{std::int64_t{150}}},
            {"currency", domain::Value{std::string{"USD"}}},
            {"active", domain::Value{true}},
        });
    protocol::InsertRequest req{domain::TableName{"transactions"}, row};
    std::string line = protocol::encode(req);
    auto parsed = protocol::parse_request(line);

    assert(parsed.has_value());
    auto* insert = std::get_if<protocol::InsertRequest>(&*parsed);
    assert(insert != nullptr);
    assert(insert->row->key() == domain::RowKey{"tx-1"});
    assert(std::get<std::int64_t>(*insert->row->find("amount")) == 150);
    assert(std::get<std::string>(*insert->row->find("currency")) == "USD");
    assert(std::get<bool>(*insert->row->find("active")) == true);
    std::cout << "test_insert_round_trip_preserves_types: OK\n";
}

void test_submit_round_trip() {
    auto row = std::make_shared<const domain::Row>(
        domain::RowKey{"tx-1"},
        std::unordered_map<std::string, domain::Value>{
            {"status", domain::Value{std::string{"NEW"}}},
            {"amount", domain::Value{std::int64_t{10}}},
        });
    protocol::SubmitRequest req{domain::TableName{"transactions"}, "tx-1", row};
    std::string line = protocol::encode(req);
    assert(line.starts_with("SUBMIT transactions tx-1 "));
    auto parsed = protocol::parse_request(line);

    assert(parsed.has_value());
    auto* submit = std::get_if<protocol::SubmitRequest>(&*parsed);
    assert(submit != nullptr);
    assert(submit->table.value() == "transactions");
    assert(submit->key == "tx-1");
    assert(std::get<std::string>(*submit->row->find("status")) == "NEW");
    assert(std::get<std::int64_t>(*submit->row->find("amount")) == 10);
    std::cout << "test_submit_round_trip: OK\n";
}

void test_subscribe_round_trip() {
    protocol::SubscribeRequest req{"sub-1", domain::TableName{"t"}, "currency = 'USD'"};
    auto parsed = protocol::parse_request(protocol::encode(req));
    assert(parsed.has_value());
    auto* sub = std::get_if<protocol::SubscribeRequest>(&*parsed);
    assert(sub != nullptr);
    assert(sub->subscription_id == "sub-1");
    assert(sub->criteria == "currency = 'USD'");
    std::cout << "test_subscribe_round_trip: OK\n";
}

void test_malformed_request_rejected() {
    auto parsed = protocol::parse_request("GARBAGE not a real command");
    assert(!parsed.has_value());
    std::cout << "test_malformed_request_rejected: OK\n";
}

void test_event_encoding() {
    auto row = std::make_shared<const domain::Row>(
        domain::RowKey{"tx-1"},
        std::unordered_map<std::string, domain::Value>{{"amount", domain::Value{std::int64_t{50}}}});
    domain::ChangeEvent event{
        .table = domain::TableName{"transactions"}, .row = row, .kind = domain::ChangeKind::Inserted};

    std::string line = protocol::encode_event(event);
    assert(line.starts_with("EVENT transactions tx-1 "));
    assert(line.find("amount=i:50") != std::string::npos);
    std::cout << "test_event_encoding: OK\n";
}

} // namespace

int main() {
    test_select_round_trip();
    test_select_empty_criteria();
    test_insert_round_trip_preserves_types();
    test_submit_round_trip();
    test_subscribe_round_trip();
    test_malformed_request_rejected();
    test_event_encoding();
    std::cout << "protocol: all tests passed\n";
    return 0;
}
