#pragma once
#include "nexus/authentication/db_path.hpp"
#include "nexus/authentication/sha256.hpp"
#include "sqlite3.h"
#include <stdexcept>
#include <string>
#include <string_view>

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

    private:
        sqlite3 *db_ = nullptr;
    };
} // namespace nexus::authentication
