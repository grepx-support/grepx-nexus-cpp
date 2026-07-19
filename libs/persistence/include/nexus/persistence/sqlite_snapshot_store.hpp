#pragma once
#include "nexus/domain/error.hpp"
#include "nexus/domain/predicate.hpp"
#include "nexus/domain/row.hpp"
#include "nexus/domain/row_key.hpp"
#include "nexus/domain/table_name.hpp"
#include "nexus/persistence/sqlite3_shim.hpp"
#include <cstdint>
#include <cstdlib>
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

// Resolves the DB path used when no explicit path is passed:
// NEXUS_DB_PATH env var if set, otherwise "nexus_data.db" relative to
// whatever the process's current working directory happens to be. That
// CWD-relative fallback is exactly what causes "where did it create the
// file" confusion — an IDE's default run-configuration working
// directory is often the build output folder, not the repo root, so
// the file silently lands somewhere unexpected. Set NEXUS_DB_PATH to an
// absolute path (matching what scripts/create_schema.py wrote to) to
// make this deterministic instead of guessing.
inline std::string default_db_path() {
    if (const char* env = std::getenv("NEXUS_DB_PATH")) return env;
    return "nexus_data.db";
}

} // namespace detail

// Default-constructible (same shape SnapshotTraits::Store is expected
// to have). See detail::default_db_path() above for how the path is
// resolved when not passed explicitly.
//
// Schema is NOT created here. nexus_rows (table_name, row_key, fields,
// version) is owned and migrated externally — see scripts/create_schema.py.
// Run it once before starting the server; this class assumes the table
// already exists and fails loud on first query if it doesn't, rather
// than silently creating a schema that could diverge from what's
// actually deployed.
class SqliteSnapshotStore {
public:
    explicit SqliteSnapshotStore(const std::string& path = detail::default_db_path())
        : path_(path) {
        if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
            throw std::runtime_error("sqlite3_open failed: " + std::string(sqlite3_errmsg(db_)));
        }
    }

    // So the server can print exactly which file it opened on startup —
    // see apps/nexus_server/src/main.cpp.
    [[nodiscard]] const std::string& path() const noexcept { return path_; }

    ~SqliteSnapshotStore() {
        if (db_) sqlite3_close(db_);
    }

    SqliteSnapshotStore(const SqliteSnapshotStore&) = delete;
    SqliteSnapshotStore& operator=(const SqliteSnapshotStore&) = delete;

    domain::Expected<void> upsert(const domain::TableName& table, domain::RowPtr row) {
        std::lock_guard lock(mutex_);
        detail::Statement stmt(db_,
            "INSERT INTO nexus_rows(table_name, row_key, fields, version) VALUES (?, ?, ?, 1) "
            "ON CONFLICT(table_name, row_key) DO UPDATE SET "
            "  fields = excluded.fields, version = nexus_rows.version + 1");

        std::string fields = detail::encode_fields(row->fields());
        sqlite3_bind_text(stmt.get(), 1, table.value().c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt.get(), 2, row->key().value().c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt.get(), 3, fields.c_str(), -1, SQLITE_TRANSIENT);

        if (sqlite3_step(stmt.get()) != SQLITE_DONE) {
            return std::unexpected(domain::Error::TableNotFound);
        }
        return {};
    }

    // ports::ConditionalWriter — optimistic-lock update used by
    // TransitionService. VersionConflict if expected_version doesn't
    // match the row's current version (includes "row doesn't exist",
    // since sqlite3_changes() is 0 either way).
    domain::Expected<std::uint64_t>
    compare_and_swap(const domain::TableName& table, const domain::RowKey& key,
                      std::uint64_t expected_version, domain::RowPtr row) {
        std::lock_guard lock(mutex_);
        detail::Statement stmt(db_,
            "UPDATE nexus_rows SET fields = ?, version = version + 1 "
            "WHERE table_name = ? AND row_key = ? AND version = ?");

        std::string fields = detail::encode_fields(row->fields());
        sqlite3_bind_text(stmt.get(), 1, fields.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt.get(), 2, table.value().c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt.get(), 3, key.value().c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt.get(), 4, static_cast<sqlite3_int64>(expected_version));

        if (sqlite3_step(stmt.get()) != SQLITE_DONE) {
            return std::unexpected(domain::Error::TableNotFound);
        }
        if (sqlite3_changes(db_) == 0) {
            return std::unexpected(domain::Error::VersionConflict);
        }
        return expected_version + 1;
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
    std::string path_;
    mutable std::mutex mutex_;
};

} // namespace nexus::persistence
