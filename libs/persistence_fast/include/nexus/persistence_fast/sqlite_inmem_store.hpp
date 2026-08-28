#pragma once
// Ultra-low-latency store: all reads/writes target an in-memory SQLite DB
// (no disk I/O on the hot path, ~microsecond write latency). A background
// std::jthread (C++20/23) checkpoints the live state to a file via the
// official sqlite3_backup API at a configurable interval (default 500 ms),
// so the data survives process restart. On construction the file is restored
// into memory if it exists, so startup is seamless.
//
// Drop-in for SqliteSnapshotStore — satisfies the same SnapshotStoreConcept
// (see below) and therefore the same SnapshotTraitsConcept used by AppEngine.
//
// Switch at CMake configure time:
//   cmake -B cmake-build-profile -DNEXUS_FAST_STORE=ON
#include "nexus/persistence/executable_dir.hpp"
#include "nexus/persistence/sqlite3_shim.hpp"
#include "nexus/persistence/sqlite_snapshot_store.hpp" // reuse detail:: helpers
#include "nexus/ports/submit_writer.hpp"
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

namespace nexus::persistence_fast {

class SqliteInMemStore {
public:
    // checkpoint_path  — where the backup file lives (env NEXUS_DB_PATH wins,
    //                    otherwise "nexus_data.db" next to the exe, same rule
    //                    as SqliteSnapshotStore so they share a file when you
    //                    switch between them without wiping the DB).
    // checkpoint_interval — how often the background thread copies memory → file.
    explicit SqliteInMemStore(
        std::string              checkpoint_path     = default_checkpoint_path(),
        std::chrono::milliseconds checkpoint_interval = std::chrono::milliseconds{500})
        : checkpoint_path_(std::move(checkpoint_path))
        , checkpoint_interval_(checkpoint_interval)
    {
        if (sqlite3_open(":memory:", &db_) != SQLITE_OK)
            throw std::runtime_error("SqliteInMemStore: sqlite3_open(:memory:) failed");

        // Exclusive locking: single process owns this in-memory DB.
        sqlite3_exec(db_, "PRAGMA locking_mode=EXCLUSIVE", nullptr, nullptr, nullptr);

        // Restore from the checkpoint file if it exists; create schema otherwise.
        if (std::filesystem::exists(checkpoint_path_))
            restore_from_file();
        else
            create_schema();

        // Background checkpoint thread — stopped automatically by jthread RAII
        // on destruction (requests stop, then joins).
        checkpoint_thread_ = std::jthread([this](std::stop_token st) {
            while (!st.stop_requested()) {
                std::this_thread::sleep_for(checkpoint_interval_);
                if (!st.stop_requested()) checkpoint_to_file();
            }
        });
    }

    ~SqliteInMemStore() {
        // stop_source fires, jthread joins, then we do one final flush so
        // the last committed state is never lost.
        checkpoint_thread_.request_stop();
        checkpoint_thread_.join();
        checkpoint_to_file();
        if (db_) sqlite3_close(db_);
    }

    SqliteInMemStore(const SqliteInMemStore&)            = delete;
    SqliteInMemStore& operator=(const SqliteInMemStore&) = delete;

    // Exposed so a runner (nexus_server main.cpp) can print where the
    // backup file lives on startup — mirrors SqliteSnapshotStore::path().
    [[nodiscard]] const std::string& path() const noexcept { return checkpoint_path_; }

    // ── ports::SnapshotWriter ────────────────────────────────────────────

    domain::Expected<void> upsert(const domain::TableName& table, domain::RowPtr row) {
        std::lock_guard lock(mutex_);
        persistence::detail::Statement stmt(db_,
            "INSERT INTO nexus_rows(table_name, row_key, fields, version) VALUES(?,?,?,1) "
            "ON CONFLICT(table_name, row_key) DO UPDATE SET "
            "  fields = excluded.fields, version = nexus_rows.version + 1");

        bind_row(stmt, table, *row);
        if (sqlite3_step(stmt.get()) != SQLITE_DONE)
            return std::unexpected(domain::Error::TableNotFound);
        return {};
    }

    domain::Expected<std::uint64_t>
    compare_and_swap(const domain::TableName& table, const domain::RowKey& key,
                     std::uint64_t expected_version, domain::RowPtr row) {
        std::lock_guard lock(mutex_);
        auto fields = persistence::detail::encode_fields(row->fields());
        persistence::detail::Statement stmt(db_,
            "UPDATE nexus_rows SET fields=?, version=version+1 "
            "WHERE table_name=? AND row_key=? AND version=?");
        sqlite3_bind_text(stmt.get(), 1, fields.c_str(),            -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt.get(), 2, table.value().c_str(),     -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt.get(), 3, key.value().c_str(),       -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt.get(), 4, static_cast<sqlite3_int64>(expected_version));
        if (sqlite3_step(stmt.get()) != SQLITE_DONE)
            return std::unexpected(domain::Error::TableNotFound);
        if (sqlite3_changes(db_) == 0)
            return std::unexpected(domain::Error::VersionConflict);
        return expected_version + 1;
    }

    // ── ports::SubmitWriter ──────────────────────────────────────────────

    // One atomic business transaction: read current status, validate
    // transition, upsert snapshot, durably record in outbox — all inside
    // one BEGIN IMMEDIATE/COMMIT. Even on an in-memory DB BEGIN IMMEDIATE
    // is correct: it prevents two concurrent submits from both reading the
    // same "current status" and both believing their transition goes first.
    domain::Expected<ports::SubmitResult>
    submit(const domain::TableName& table, const domain::RowKey& key, domain::RowPtr row,
           const std::function<bool(std::string_view, std::string_view)>& is_allowed) {
        std::lock_guard lock(mutex_);

        if (sqlite3_exec(db_, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) != SQLITE_OK)
            return std::unexpected(domain::Error::TableNotFound);

        // Read current row (if any)
        std::string      current_status;
        std::uint64_t    current_version = 0;
        bool             exists          = false;
        {
            persistence::detail::Statement s(db_,
                "SELECT fields, version FROM nexus_rows WHERE table_name=? AND row_key=?");
            sqlite3_bind_text(s.get(), 1, table.value().c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(s.get(), 2, key.value().c_str(),   -1, SQLITE_TRANSIENT);
            if (sqlite3_step(s.get()) == SQLITE_ROW) {
                exists          = true;
                current_version = static_cast<std::uint64_t>(sqlite3_column_int64(s.get(), 1));
                std::string ft(reinterpret_cast<const char*>(sqlite3_column_text(s.get(), 0)),
                               static_cast<std::size_t>(sqlite3_column_bytes(s.get(), 0)));
                auto fields = persistence::detail::decode_fields(ft);
                if (auto it = fields.find("status"); it != fields.end())
                    if (const auto* sv = std::get_if<std::string>(&it->second))
                        current_status = *sv;
            }
        }

        std::string to_status;
        if (auto it = row->fields().find("status"); it != row->fields().end())
            if (const auto* sv = std::get_if<std::string>(&it->second)) to_status = *sv;

        if (exists && !is_allowed(current_status, to_status)) {
            sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
            return std::unexpected(domain::Error::InvalidTransition);
        }

        // Upsert snapshot row
        auto fields_text = persistence::detail::encode_fields(row->fields());
        {
            persistence::detail::Statement s(db_,
                "INSERT INTO nexus_rows(table_name, row_key, fields, version) VALUES(?,?,?,1) "
                "ON CONFLICT(table_name, row_key) DO UPDATE SET "
                "  fields = excluded.fields, version = nexus_rows.version + 1");
            sqlite3_bind_text(s.get(), 1, table.value().c_str(),  -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(s.get(), 2, key.value().c_str(),    -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(s.get(), 3, fields_text.c_str(),    -1, SQLITE_TRANSIENT);
            if (sqlite3_step(s.get()) != SQLITE_DONE) {
                sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
                return std::unexpected(domain::Error::TableNotFound);
            }
        }

        // Outbox row (from_service / to_service left NULL — populated by the
        // Python dispatch layer once services identify themselves via the
        // to_service / from_service subscription criteria).
        std::uint64_t outbox_id = 0;
        {
            persistence::detail::Statement s(db_,
                "INSERT INTO outbox"
                "  (table_name, transaction_id, from_status, to_status, fields, occurred_at)"
                "  VALUES(?,?,?,?,?,strftime('%Y-%m-%dT%H:%M:%fZ','now'))");
            sqlite3_bind_text(s.get(), 1, table.value().c_str(),  -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(s.get(), 2, key.value().c_str(),    -1, SQLITE_TRANSIENT);
            if (exists) sqlite3_bind_text(s.get(), 3, current_status.c_str(), -1, SQLITE_TRANSIENT);
            else        sqlite3_bind_null(s.get(), 3);
            sqlite3_bind_text(s.get(), 4, to_status.c_str(),      -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(s.get(), 5, fields_text.c_str(),    -1, SQLITE_TRANSIENT);
            if (sqlite3_step(s.get()) != SQLITE_DONE) {
                sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
                return std::unexpected(domain::Error::TableNotFound);
            }
            outbox_id = static_cast<std::uint64_t>(sqlite3_last_insert_rowid(db_));
        }

        if (sqlite3_exec(db_, "COMMIT", nullptr, nullptr, nullptr) != SQLITE_OK)
            return std::unexpected(domain::Error::TableNotFound);

        return ports::SubmitResult{
            .version     = exists ? current_version + 1 : 1,
            .from_status = current_status,
            .created     = !exists,
            .outbox_id   = outbox_id,
        };
    }

    void mark_published(std::uint64_t outbox_id) {
        std::lock_guard lock(mutex_);
        persistence::detail::Statement s(db_,
            "UPDATE outbox SET published_at=strftime('%Y-%m-%dT%H:%M:%fZ','now') WHERE id=?");
        sqlite3_bind_int64(s.get(), 1, static_cast<sqlite3_int64>(outbox_id));
        sqlite3_step(s.get());
    }

    // ── ports::SnapshotReader ────────────────────────────────────────────

    [[nodiscard]] std::vector<domain::RowPtr>
    select(const domain::TableName& table, const domain::PredicatePtr& predicate) const {
        std::lock_guard lock(mutex_);
        std::vector<domain::RowPtr> results;
        persistence::detail::Statement s(db_,
            "SELECT row_key, fields FROM nexus_rows WHERE table_name=?");
        sqlite3_bind_text(s.get(), 1, table.value().c_str(), -1, SQLITE_TRANSIENT);
        while (sqlite3_step(s.get()) == SQLITE_ROW) {
            std::string k(reinterpret_cast<const char*>(sqlite3_column_text(s.get(), 0)),
                          static_cast<std::size_t>(sqlite3_column_bytes(s.get(), 0)));
            std::string ft(reinterpret_cast<const char*>(sqlite3_column_text(s.get(), 1)),
                           static_cast<std::size_t>(sqlite3_column_bytes(s.get(), 1)));
            auto row = std::make_shared<const domain::Row>(
                domain::RowKey{k}, persistence::detail::decode_fields(ft));
            if (domain::matches(predicate, *row)) results.push_back(std::move(row));
        }
        return results;
    }

private:
    // ── helpers ─────────────────────────────────────────────────────────

    static std::string default_checkpoint_path() {
        if (const char* env = std::getenv("NEXUS_DB_PATH")) return env;
        return persistence::executable_dir() + "/nexus_data.db";
    }

    // Helper: bind table/key/fields onto positions 1/2/3 of a prepared stmt.
    static void bind_row(persistence::detail::Statement& stmt,
                         const domain::TableName& table,
                         const domain::Row&       row) {
        auto fields = persistence::detail::encode_fields(row.fields());
        sqlite3_bind_text(stmt.get(), 1, table.value().c_str(),  -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt.get(), 2, row.key().value().c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt.get(), 3, fields.c_str(),          -1, SQLITE_TRANSIENT);
    }

    // Creates the full schema in the freshly-opened in-memory DB (nothing
    // to restore from — this is the very first run, or the checkpoint file
    // was deleted). The outbox schema includes from_service / to_service
    // (added 2026-08) so the in-mem store is always in sync with the Python
    // model even before the file checkpoint is taken.
    void create_schema() {
        const char* ddl =
            "CREATE TABLE IF NOT EXISTS nexus_rows("
            "  table_name TEXT NOT NULL,"
            "  row_key    TEXT NOT NULL,"
            "  fields     TEXT NOT NULL,"
            "  version    INTEGER NOT NULL DEFAULT 1,"
            "  PRIMARY KEY(table_name, row_key));"

            "CREATE TABLE IF NOT EXISTS outbox("
            "  id             INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  table_name     TEXT NOT NULL,"
            "  transaction_id TEXT NOT NULL,"
            "  from_status    TEXT,"
            "  to_status      TEXT NOT NULL,"
            "  from_service   TEXT,"
            "  to_service     TEXT,"
            "  fields         TEXT NOT NULL,"
            "  occurred_at    TEXT NOT NULL,"
            "  published_at   TEXT);"

            "CREATE INDEX IF NOT EXISTS idx_outbox_unpublished"
            "  ON outbox(published_at) WHERE published_at IS NULL;";

        sqlite3_exec(db_, ddl, nullptr, nullptr, nullptr);
    }

    // sqlite3_backup: file → memory (restore on startup).
    void restore_from_file() {
        sqlite3* src = nullptr;
        if (sqlite3_open_v2(checkpoint_path_.c_str(), &src,
                            SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
            sqlite3_close(src);
            create_schema(); // unreadable checkpoint — start fresh
            return;
        }
        auto* bkp = sqlite3_backup_init(db_, "main", src, "main");
        if (bkp) { sqlite3_backup_step(bkp, -1); sqlite3_backup_finish(bkp); }
        sqlite3_close(src);
    }

    // sqlite3_backup: memory → file (called by background thread + destructor).
    // Best-effort: if the path is bad the write is simply skipped.
    void checkpoint_to_file() {
        sqlite3* dst = nullptr;
        if (sqlite3_open(checkpoint_path_.c_str(), &dst) != SQLITE_OK) {
            sqlite3_close(dst);
            return;
        }
        // Tune the file DB for fast sequential bulk writes (checkpoint is a
        // full page copy, not individual row writes).
        sqlite3_exec(dst, "PRAGMA journal_mode=WAL",       nullptr, nullptr, nullptr);
        sqlite3_exec(dst, "PRAGMA synchronous=NORMAL",     nullptr, nullptr, nullptr);

        {
            std::lock_guard lock(mutex_);
            auto* bkp = sqlite3_backup_init(dst, "main", db_, "main");
            if (bkp) { sqlite3_backup_step(bkp, -1); sqlite3_backup_finish(bkp); }
        }
        sqlite3_close(dst);
    }

    sqlite3*                   db_                 = nullptr;
    std::string                checkpoint_path_;
    std::chrono::milliseconds  checkpoint_interval_;
    mutable std::mutex         mutex_;
    std::jthread               checkpoint_thread_; // stopped + joined before db_ is closed
};

// ── C++23 concept: the full store contract ───────────────────────────────────
//
// Both SqliteSnapshotStore and SqliteInMemStore must satisfy this. A future
// Redis store, a mock test double, or any other backend can be plugged in by
// satisfying this concept alone — nothing else in the engine changes.
template <typename S>
concept SnapshotStoreConcept =
    requires(S& store, const S& cstore,
             const domain::TableName& table,
             const domain::RowKey&    key,
             domain::RowPtr           row,
             std::uint64_t            version,
             const domain::PredicatePtr& pred,
             const std::function<bool(std::string_view, std::string_view)>& is_allowed,
             std::uint64_t outbox_id)
    {
        { store.upsert(table, row)                          } -> std::same_as<domain::Expected<void>>;
        { cstore.select(table, pred)                        } -> std::same_as<std::vector<domain::RowPtr>>;
        { store.compare_and_swap(table, key, version, row) } -> std::same_as<domain::Expected<std::uint64_t>>;
        { store.submit(table, key, row, is_allowed)         } -> std::same_as<domain::Expected<ports::SubmitResult>>;
        { store.mark_published(outbox_id)                  } -> std::same_as<void>;
        { cstore.path()                                    } -> std::same_as<const std::string&>;
    };

// Compile-time proof that SqliteInMemStore satisfies the contract.
static_assert(SnapshotStoreConcept<SqliteInMemStore>);

} // namespace nexus::persistence_fast
