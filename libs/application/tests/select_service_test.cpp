#include "nexus/application/select_service.hpp"
#include <cassert>
#include <iostream>

using namespace nexus;

namespace {

// Local fake — proves SelectService compiles against anything satisfying
// SnapshotReader, not just nexus_infra's concrete SnapshotStore.
struct FakeReader {
    std::vector<domain::RowPtr> rows;

    [[nodiscard]] std::vector<domain::RowPtr>
    select(const domain::TableName&, const domain::PredicatePtr& predicate) const {
        std::vector<domain::RowPtr> out;
        for (const auto& row : rows) {
            if (domain::matches(predicate, *row)) out.push_back(row);
        }
        return out;
    }
};

domain::RowPtr row_with(std::int64_t amount) {
    return std::make_shared<const domain::Row>(
        domain::RowKey{"k"}, std::unordered_map<std::string, domain::Value>{{"amount", domain::Value{amount}}});
}

void test_select_delegates_to_reader() {
    FakeReader reader{.rows = {row_with(50), row_with(150)}};
    application::SelectService select(reader);

    auto pred = std::make_shared<const domain::Predicate>(
        domain::Predicate{.node = domain::Comparison{"amount", domain::CompareOp::Gt,
                                                       domain::Value{std::int64_t{100}}}});
    auto results = select.execute(domain::TableName{"t"}, pred);
    assert(results.size() == 1);
    std::cout << "test_select_delegates_to_reader: OK\n";
}

} // namespace

int main() {
    test_select_delegates_to_reader();
    std::cout << "application/select_service: all tests passed\n";
    return 0;
}
