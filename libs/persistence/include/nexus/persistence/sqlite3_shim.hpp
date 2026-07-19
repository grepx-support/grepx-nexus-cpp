#pragma once
// This repo vendors the full SQLite amalgamation (external/sqlite-amalgamation-3530300/),
// which is already on the include path for anything linking the sqlite3
// target (see external/sqlite-amalgamation-3530300/CMakeLists.txt —
// target_include_directories(sqlite3 PUBLIC .)). Use the real header
// rather than hand-declaring a subset of the C API.
#include "sqlite3.h"
