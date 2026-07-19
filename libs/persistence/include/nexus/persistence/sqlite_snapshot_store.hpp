#pragma once
#include "nexus/domain/error.hpp"
#include "nexus/domain/predicate.hpp"
#include "nexus/domain/row.hpp"
#include "nexus/domain/table_name.hpp"
#include "nexus/persistence/sqlite3_shim.hpp"
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace nexus::persistence {

namespace detail {

// Same tag scheme as nexus_protocol's wire codec (i:/d:/b:/s:/n:), but
// implemented independently here — persistence and protocol are
// unrelated concerns and shouldn't depend on each other just because
// they happen to both need "serialize a Value".
inline std::string encode_value(const domain::Value& value) {
    return std::visit(
        [](const auto& v) -> std::string {
            using V = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<V, std::monostate>) return "n:";
            else if constexpr (std::is_same_v<V, std::int64_t>) return "i:" + std::to_string(v);
            else if constexpr (std::is_same_v<V, double>) return "d:" + std::to_string(v);
            else if constexpr (std::is_same_v<V, bool>) return v ? "b:1" : "b:0";
            else if constexpr (std::is_same_v<V, std::string>) return "s:" + v;
        },
        value);
}

inline domain::Value decode_value(const std::string& text) {
    if (text.size() < 2) return domain::Value{std::monostate{}};
    char tag = text[0];
    std::string body = text.substr(2);
    switch (tag) {
        case 'i': return domain::Value{static_cast<std::int64_t>(std::stoll(body))};
        case 'd': return domain::Value{std::stod(body)};
        case 'b': return domain::Value{body == "1"};
        case 's': return domain::Value{body};
        default: return domain::Value{std::monostate{}};
    }
}

inline std::string encode_fields(const std::unordered_map<std::string, domain::Value>& fields) {
    std::string out;
    bool first = true;
    for (const auto& [name, value] : fields) {
        if (!first) out += '\x1f'; // unit separator — unlikely in field names/values
        out += name + "\x1e" + encode_value(value);
        first = false;
    }
    return out;
}

inline std::unordered_map<std::string, domain::Value> decode_fields(const std::string& text) {
    std::unordered_map<std::string, domain::Value> fields;
    std::stringstream ss(text);
    std::string pair;
    while (std::getline(ss, pair, '\x1f')) {
        auto sep = pair.find('\x1e');
        if (sep == std::string::npos) continue;
        fields[pair.substr(0, sep)] = decode_value(pair.substr(sep + 1));
    }
    return fields;
}

// RAII wrapper so a thrown exception or early return can't leak a
// prepared statement.
class Statement {
public:
    Statement(sqlite3* db, const std::string& sql) {
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt_, nullptr) != SQLITE_OK) {
            throw std::runtime_error("sqlite3_prepare_v2 failed");
        }
    }
    ~Statement() { sqlite3_finalize(stmt_); }
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    sqlite3_stmt* get() const { return stmt_; }

private:
    sqlite3_stmt* stmt_ = nullptr;
};

} // namespace detail

// Default-constructible (same shape SnapshotTraits::Store is expected
// to have), opening a fixed on-disk file in the current working
// directory. A production version would take the path through
// NetworkTraits/config instead of hardcoding it — see README.
class SqliteSnapshotStore {
public:
    explicit SqliteSnapshotStore(const std::string& path = "nexus_data.db") {
        if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
            throw std::runtime_error("sqlite3_open failed: " + std::string(sqlite3_errmsg(db_)));
        }
        const char* create_sql =
            "CREATE TABLE IF NOT EXISTS nexus_rows ("
            "  table_name TEXT NOT NULL,"
            "  row_key TEXT NOT NULL,"
            "  fields TEXT NOT NULL,"
            "  PRIMARY KEY (table_name, row_key)"
            ")";
        char* err = nullptr;
        if (sqlite3_exec(db_, create_sql, nullptr, nullptr, &err) != SQLITE_OK) {
            std::string message = err ? err : "unknown error";
            sqlite3_free(err);
            throw std::runtime_error("CREATE TABLE failed: " + message);
        }
    }

    ~SqliteSnapshotStore() {
        if (db_) sqlite3_close(db_);
    }

    SqliteSnapshotStore(const SqliteSnapshotStore&) = delete;
    SqliteSnapshotStore& operator=(const SqliteSnapshotStore&) = delete;

    domain::Expected<void> upsert(const domain::TableName& table, domain::RowPtr row) {
        std::lock_guard lock(mutex_);
        detail::Statement stmt(db_,
            "INSERT INTO nexus_rows(table_name, row_key, fields) VALUES (?, ?, ?) "
            "ON CONFLICT(table_name, row_key) DO UPDATE SET fields = excluded.fields");

        std::string fields = detail::encode_fields(row->fields());
        sqlite3_bind_text(stmt.get(), 1, table.value().c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt.get(), 2, row->key().value().c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt.get(), 3, fields.c_str(), -1, SQLITE_TRANSIENT);

        if (sqlite3_step(stmt.get()) != SQLITE_DONE) {
            return std::unexpected(domain::Error::TableNotFound);
        }
        return {};
    }

    [[nodiscard]] std::vector<domain::RowPtr>
    select(const domain::TableName& table, const domain::PredicatePtr& predicate) const {
        std::lock_guard lock(mutex_);
        std::vector<domain::RowPtr> results;

        detail::Statement stmt(db_, "SELECT row_key, fields FROM nexus_rows WHERE table_name = ?");
        sqlite3_bind_text(stmt.get(), 1, table.value().c_str(), -1, SQLITE_TRANSIENT);

        while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
            std::string key(reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 0)),
                             static_cast<std::size_t>(sqlite3_column_bytes(stmt.get(), 0)));
            std::string fields_text(reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 1)),
                                     static_cast<std::size_t>(sqlite3_column_bytes(stmt.get(), 1)));

            auto row = std::make_shared<const domain::Row>(domain::RowKey{key},
                                                             detail::decode_fields(fields_text));
            // Predicate evaluation happens in C++ after loading the
            // table's rows — this MVP doesn't translate Predicate into
            // a SQL WHERE clause. Fine at MVP scale; a real deployment
            // would push down comparisons it can and filter the rest.
            if (domain::matches(predicate, *row)) results.push_back(row);
        }
        return results;
    }

private:
    sqlite3* db_ = nullptr;
    mutable std::mutex mutex_;
};

} // namespace nexus::persistence
