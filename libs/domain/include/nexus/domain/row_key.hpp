#pragma once
#include <string>
#include <functional>

namespace nexus::domain {

// Strong type wrapping a row's primary key so it can never be confused
// with a plain std::string (e.g. a table name or field name).
class RowKey {
public:
    explicit RowKey(std::string value) : value_(std::move(value)) {}

    [[nodiscard]] const std::string& value() const noexcept { return value_; }

    friend bool operator==(const RowKey&, const RowKey&) = default;

private:
    std::string value_;
};

} // namespace nexus::domain

template <>
struct std::hash<nexus::domain::RowKey> {
    std::size_t operator()(const nexus::domain::RowKey& key) const noexcept {
        return std::hash<std::string>{}(key.value());
    }
};
