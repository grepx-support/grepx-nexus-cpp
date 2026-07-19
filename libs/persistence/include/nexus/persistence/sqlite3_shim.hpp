#pragma once
// Minimal subset of SQLite's stable C ABI. This sandbox has the SQLite
// runtime library installed but not the -dev headers (no internet
// access to apt/vcpkg to fetch them here). SQLite's C API has been
// binary-stable since 3.x, so hand-declaring the handful of functions
// this adapter needs and linking against libsqlite3.so.0 directly is
// safe and correct.
//
// On your machine: `apt install libsqlite3-dev` (or via vcpkg) and
// replace this header's contents with `#include <sqlite3.h>` — nothing
// else in sqlite_snapshot_store.hpp changes.

extern "C" {

struct sqlite3;
struct sqlite3_stmt;

int sqlite3_open(const char* filename, sqlite3** db);
int sqlite3_close(sqlite3* db);
int sqlite3_exec(sqlite3* db, const char* sql,
                  int (*callback)(void*, int, char**, char**), void* arg, char** errmsg);
void sqlite3_free(void* ptr);

int sqlite3_prepare_v2(sqlite3* db, const char* sql, int n_bytes, sqlite3_stmt** stmt,
                        const char** tail);
int sqlite3_step(sqlite3_stmt* stmt);
int sqlite3_finalize(sqlite3_stmt* stmt);
int sqlite3_reset(sqlite3_stmt* stmt);

int sqlite3_bind_text(sqlite3_stmt* stmt, int index, const char* text, int n_bytes,
                       void (*destructor)(void*));
int sqlite3_bind_int64(sqlite3_stmt* stmt, int index, long long value);
int sqlite3_bind_double(sqlite3_stmt* stmt, int index, double value);
int sqlite3_bind_null(sqlite3_stmt* stmt, int index);

int sqlite3_column_type(sqlite3_stmt* stmt, int col);
const unsigned char* sqlite3_column_text(sqlite3_stmt* stmt, int col);
long long sqlite3_column_int64(sqlite3_stmt* stmt, int col);
double sqlite3_column_double(sqlite3_stmt* stmt, int col);
int sqlite3_column_bytes(sqlite3_stmt* stmt, int col);

const char* sqlite3_errmsg(sqlite3* db);

} // extern "C"

// Result codes and column type tags we reference by name.
inline constexpr int SQLITE_OK = 0;
inline constexpr int SQLITE_ROW = 100;
inline constexpr int SQLITE_DONE = 101;
inline constexpr int SQLITE_INTEGER = 1;
inline constexpr int SQLITE_FLOAT = 2;
inline constexpr int SQLITE_TEXT = 3;
inline constexpr int SQLITE_NULL = 5;
inline constexpr int SQLITE_STATIC_UNUSED = 0; // kept only for documentation parity with sqlite3.h
using sqlite3_destructor_type = void (*)(void*);
inline sqlite3_destructor_type SQLITE_TRANSIENT = reinterpret_cast<sqlite3_destructor_type>(-1);
