/*
 * Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
 * This file is part of the GrepX platform and is proprietary and confidential.
 * Unauthorised copying, use, modification or distribution is strictly prohibited.
 */

#pragma once
#include "nexus/domain/error.hpp"
#include "nexus/domain/predicate.hpp"
#include "nexus/domain/row.hpp"
#include "nexus/domain/row_key.hpp"
#include "nexus/domain/table_name.hpp"
#include "nexus/persistence/executable_dir.hpp"
#include "nexus/persistence/sqlite3_shim.hpp"
#include "nexus/ports/submit_writer.hpp"
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string_view>
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
//   1. NEXUS_DB_PATH env var, if set — always wins, for whoever wants
//      explicit control.
//   2. Otherwise, "nexus_data.db" next to the running executable
//      itself — NOT the process's current working directory. CWD
//      varies by how you launch the binary (double-click, an IDE's
//      run-config default, a terminal cd'd somewhere else); the exe's
//      own location does not. This is what makes "where did it put the
//      file" have exactly one answer regardless of how nexus_server
//      was started — see executable_dir().
inline std::string default_db_path() {
    if (const char* env = std::getenv("NEXUS_DB_PATH")) return env;
    return executable_dir() + "/nexus_data.db";
}

} // namespace detail

// Default-constructible (same shape SnapshotTraits::Store is expected
// to have). See detail::default_db_path() above for how the path is
// resolved when not passed explicitly.
//
// Schema is NOT created here. nexus_rows (table_name, row_key, fields,
// version) is owned and migrated externally — see python/create_schema.py (applied via scripts/setup.sh).
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

    // ports::SubmitWriter — one atomic unit covering everything a
    // client-visible "submit a transaction" call needs: read the row's
    // current status (if it exists), validate the transition, upsert
    // the snapshot, and durably record the attempt in outbox — all
    // inside one BEGIN IMMEDIATE/COMMIT, so a caller (SubmitService)
    // never has to make two separate network round trips for what is
    // conceptually one business event, and can never observe a
    // half-applied one (rollback on any failure, including an illegal
    // transition).
    //
    // BEGIN IMMEDIATE (not the bare "BEGIN" a deferred transaction
    // would use) takes SQLite's write lock up front, before the read —
    // otherwise two concurrent submits could both read the same
    // "current status" and both believe their transition is the one
    // being applied first.
    domain::Expected<ports::SubmitResult>
    submit(const domain::TableName& table, const domain::RowKey& key, domain::RowPtr row,
           const std::function<bool(std::string_view, std::string_view)>& is_allowed) {
        std::lock_guard lock(mutex_);

        if (sqlite3_exec(db_, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) != SQLITE_OK) {
            return std::unexpected(domain::Error::TableNotFound);
        }

        std::string current_status;
        std::uint64_t current_version = 0;
        bool exists = false;
        {
            detail::Statement stmt(db_,
                "SELECT fields, version FROM nexus_rows WHERE table_name = ? AND row_key = ?");
            sqlite3_bind_text(stmt.get(), 1, table.value().c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt.get(), 2, key.value().c_str(), -1, SQLITE_TRANSIENT);
            if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
                exists = true;
                std::string fields_text(reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 0)),
                                         static_cast<std::size_t>(sqlite3_column_bytes(stmt.get(), 0)));
                current_version = static_cast<std::uint64_t>(sqlite3_column_int64(stmt.get(), 1));
                auto fields = detail::decode_fields(fields_text);
                if (auto it = fields.find("status"); it != fields.end()) {
                    if (const auto* s = std::get_if<std::string>(&it->second)) current_status = *s;
                }
            }
        }

        std::string to_status;
        if (auto it = row->fields().find("status"); it != row->fields().end()) {
            if (const auto* s = std::get_if<std::string>(&it->second)) to_status = *s;
        }

        // No prior row: any initial status is accepted, nothing to
        // validate a transition from. A prior row: the caller-supplied
        // state machine has the final say.
        if (exists && !is_allowed(current_status, to_status)) {
            sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
            return std::unexpected(domain::Error::InvalidTransition);
        }

        std::string fields_text = detail::encode_fields(row->fields());
        {
            detail::Statement stmt(db_,
                "INSERT INTO nexus_rows(table_name, row_key, fields, version) VALUES (?, ?, ?, 1) "
                "ON CONFLICT(table_name, row_key) DO UPDATE SET "
                "  fields = excluded.fields, version = nexus_rows.version + 1");
            sqlite3_bind_text(stmt.get(), 1, table.value().c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt.get(), 2, key.value().c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt.get(), 3, fields_text.c_str(), -1, SQLITE_TRANSIENT);
            if (sqlite3_step(stmt.get()) != SQLITE_DONE) {
                sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
                return std::unexpected(domain::Error::TableNotFound);
            }
        }

        std::uint64_t outbox_id = 0;
        {
            detail::Statement stmt(db_,
                "INSERT INTO outbox(table_name, transaction_id, from_status, to_status, fields, occurred_at) "
                "VALUES (?, ?, ?, ?, ?, strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))");
            sqlite3_bind_text(stmt.get(), 1, table.value().c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt.get(), 2, key.value().c_str(), -1, SQLITE_TRANSIENT);
            if (exists) {
                sqlite3_bind_text(stmt.get(), 3, current_status.c_str(), -1, SQLITE_TRANSIENT);
            } else {
                sqlite3_bind_null(stmt.get(), 3);
            }
            sqlite3_bind_text(stmt.get(), 4, to_status.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt.get(), 5, fields_text.c_str(), -1, SQLITE_TRANSIENT);
            if (sqlite3_step(stmt.get()) != SQLITE_DONE) {
                sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
                return std::unexpected(domain::Error::TableNotFound);
            }
            outbox_id = static_cast<std::uint64_t>(sqlite3_last_insert_rowid(db_));
        }

        if (sqlite3_exec(db_, "COMMIT", nullptr, nullptr, nullptr) != SQLITE_OK) {
            return std::unexpected(domain::Error::TableNotFound);
        }

        return ports::SubmitResult{
            .version = exists ? current_version + 1 : 1,
            .from_status = current_status,
            .created = !exists,
            .outbox_id = outbox_id,
        };
    }

    // Best-effort bookkeeping, deliberately outside submit()'s
    // transaction — delivering to a live in-memory subscriber isn't a
    // SQLite operation, so it can never be made atomic with the write
    // above (that's the whole reason the outbox exists: to durably
    // record the event regardless of whether delivery succeeds). A row
    // that never gets marked published just means "was there a live
    // subscriber to tell" is still open — the write itself already
    // committed either way.
    void mark_published(std::uint64_t outbox_id) {
        std::lock_guard lock(mutex_);
        detail::Statement stmt(db_,
            "UPDATE outbox SET published_at = strftime('%Y-%m-%dT%H:%M:%fZ', 'now') WHERE id = ?");
        sqlite3_bind_int64(stmt.get(), 1, static_cast<sqlite3_int64>(outbox_id));
        sqlite3_step(stmt.get());
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
