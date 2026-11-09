/*
 * Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
 * This file is part of the GrepX platform and is proprietary and confidential.
 * Unauthorised copying, use, modification or distribution is strictly prohibited.
 */

#pragma once
#include "nexus/authentication/db_path.hpp"
#include "nexus/authentication/sha256.hpp"
#include "sqlite3.h"
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace nexus::authentication {
    // Looks users up in the "users" table (username, password_hash) — same
    // DB nexus_server already opens for nexus_rows. Schema/seeding lives in
    // python/create_schema.py + python/seed_user.py (run via scripts/setup.sh), same split as
    // everything else: this class never creates or writes rows.
    class DbAuthenticator {
    public:
        explicit DbAuthenticator(const std::string &path = detail::default_db_path()) {
            if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
                throw std::runtime_error("DbAuthenticator: sqlite3_open failed");
            }
            sqlite3_busy_timeout(db_, 5000);   // shares the file with the row store: wait for a writer, never fail at once
            load_owners();
        }

        ~DbAuthenticator() { if (db_) sqlite3_close(db_); }

        DbAuthenticator(const DbAuthenticator &) = delete;

        DbAuthenticator &operator=(const DbAuthenticator &) = delete;

        [[nodiscard]] bool authenticate(std::string_view username, std::string_view password) const {
            sqlite3_stmt *stmt = nullptr;
            if (sqlite3_prepare_v2(db_, "SELECT password_hash FROM users WHERE username = ?", -1, &stmt, nullptr) !=
                SQLITE_OK) {
                return false;
            }
            sqlite3_bind_text(stmt, 1, username.data(), static_cast<int>(username.size()), SQLITE_TRANSIENT);

            bool ok = false;
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                std::string stored(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0)),
                                   static_cast<std::size_t>(sqlite3_column_bytes(stmt, 0)));
                ok = (stored == detail::sha256_hex(password));
            }
            sqlite3_finalize(stmt);
            return ok;
        }

        // Table ownership (least privilege): a table listed in table_owners may only be
        // written (INSERT / TRANSITION / SUBMIT) by the usernames listed for it. Tables
        // nobody owns - request tables every service submits to - stay open, and a
        // database without table_owners (older setups) allows every write as before.
        // Loaded once at start (read-only afterwards, so no locking and no SQL per write);
        // `scripts/run.sh nexus-accounts` then a server restart picks up changes.
        [[nodiscard]] bool can_write(std::string_view username, std::string_view table) const {
            auto it = owners_.find(std::string(table));
            if (it == owners_.end()) return true;
            for (const auto &owner: it->second) if (owner == username) return true;
            return false;
        }

    private:
        void load_owners() {
            sqlite3_stmt *stmt = nullptr;
            if (sqlite3_prepare_v2(db_, "SELECT table_name, username FROM table_owners", -1, &stmt, nullptr) != SQLITE_OK) {
                return;   // no table_owners table: ownership not configured
            }
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                owners_[reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0))].emplace_back(
                    reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1)));
            }
            sqlite3_finalize(stmt);
        }

        sqlite3 *db_ = nullptr;
        std::unordered_map<std::string, std::vector<std::string>> owners_;
    };
} // namespace nexus::authentication
