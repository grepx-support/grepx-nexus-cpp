/*
 * Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
 * This file is part of the GrepX platform and is proprietary and confidential.
 * Unauthorised copying, use, modification or distribution is strictly prohibited.
 */

#pragma once
#include "nexus/authentication/db_path.hpp" // reuse — same DB, same NEXUS_DB_PATH-first rule
#include "nexus/session/login_audit.hpp"
#include "sqlite3.h"
#include <ctime>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace nexus::session {

// Appends to / updates the "user_login" table — full history, not just
// current state (InMemorySessionRegistry already covers "who's active
// right now"; this is the durable audit trail: WHO logged in, AS WHAT
// application, WITH what access mode, WHEN, and WHEN they logged off).
// Schema lives in python/models.py (applied via scripts/setup.sh), same split as everywhere else —
// this class never creates or alters schema, only reads/writes rows.
//
// Deliberately does NOT store the password (plaintext or hashed) here —
// an audit/login table is not the place for credentials, even hashed
// ones widen the blast radius if this table ever leaks or gets
// exported. auth_method records HOW they authenticated, not WITH WHAT.
class SqliteLoginAudit {
public:
    explicit SqliteLoginAudit(const std::string& path = authentication::detail::default_db_path()) {
        if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
            throw std::runtime_error("SqliteLoginAudit: sqlite3_open failed");
        }
    }

    ~SqliteLoginAudit() { if (db_) sqlite3_close(db_); }
    SqliteLoginAudit(const SqliteLoginAudit&) = delete;
    SqliteLoginAudit& operator=(const SqliteLoginAudit&) = delete;

    void record_login(SessionId id, const Session& s) {
        std::lock_guard lock(mutex_);
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_,
                "INSERT INTO user_login(username, application_name, access_mode, auth_method, login_time, status) "
                "VALUES (?, ?, ?, 'password', ?, 'LOGGED_IN')", -1, &stmt, nullptr) != SQLITE_OK) {
            return;
        }
        std::string now = current_timestamp();
        sqlite3_bind_text(stmt, 1, s.username.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, s.application_name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, s.access_mode.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, now.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        row_id_by_session_[id] = sqlite3_last_insert_rowid(db_);
    }

    void record_logout(SessionId id) {
        std::lock_guard lock(mutex_);
        auto it = row_id_by_session_.find(id);
        if (it == row_id_by_session_.end()) return;

        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_,
                "UPDATE user_login SET status = 'LOGGED_OFF', logout_time = ? WHERE rowid = ?",
                -1, &stmt, nullptr) == SQLITE_OK) {
            std::string now = current_timestamp();
            sqlite3_bind_text(stmt, 1, now.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int64(stmt, 2, it->second);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
        row_id_by_session_.erase(it);
    }

    // Called exactly once, at server startup before the accept loop
    // begins (see run_server() in apps/nexus_server/src/main.cpp) —
    // never during normal operation, where a LOGGED_IN row is exactly
    // what it should be. Distinct status ('ORPHANED', not 'LOGGED_OFF')
    // so the audit trail stays honest: this was never a real logout,
    // just this process noticing leftover rows from a previous one that
    // didn't get to close them out gracefully.
    void reconcile_orphaned_sessions() {
        std::lock_guard lock(mutex_);
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_,
                "UPDATE user_login SET status = 'ORPHANED', logout_time = ? WHERE status = 'LOGGED_IN'",
                -1, &stmt, nullptr) != SQLITE_OK) {
            return;
        }
        std::string now = current_timestamp();
        sqlite3_bind_text(stmt, 1, now.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

private:
    static std::string current_timestamp() {
        std::time_t t = std::time(nullptr);
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::gmtime(&t));
        return buf;
    }

    sqlite3* db_ = nullptr;
    std::mutex mutex_;
    std::unordered_map<SessionId, sqlite3_int64> row_id_by_session_;
};

static_assert(LoginAudit<SqliteLoginAudit>);

} // namespace nexus::session
