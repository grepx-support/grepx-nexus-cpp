#pragma once
#include <string>
#include <functional>

namespace nexus::domain {

// Strong type wrapping a table name so it can never be confused with a
// RowKey or a plain field name at a call site.
class TableName {
public:
    explicit TableName(std::string value) : value_(std::move(value)) {}

    [[nodiscard]] const std::string& value() const noexcept { return value_; }

    friend bool operator==(const TableName&, const TableName&) = default;

private:
    std::string value_;
};

} // namespace nexus::domain

template <>
struct std::hash<nexus::domain::TableName> {
    std::size_t operator()(const nexus::domain::TableName& name) const noexcept {
        return std::hash<std::string>{}(name.value());
    }
};
