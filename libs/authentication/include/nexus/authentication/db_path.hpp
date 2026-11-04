/*
 * Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
 * This file is part of the GrepX platform and is proprietary and confidential.
 * Unauthorised copying, use, modification or distribution is strictly prohibited.
 */

#pragma once
#include <cstdlib>
#include <string>

// Deliberately NOT reusing nexus::persistence::executable_dir()/
// default_db_path() here — depending on libs/persistence would create a
// cycle (persistence depends on traits, traits needs authentication for
// AuthTraits). This is the same resolution logic, duplicated on purpose:
// small, dependency-free, and identical behavior to
// nexus/persistence/executable_dir.hpp. If the resolution rule ever
// changes, update both.

#if defined(_WIN32)
    #include <windows.h>
#else
    #include <climits>
    #include <unistd.h>
#endif

namespace nexus::authentication::detail {

inline std::string executable_dir() {
    std::string path;

#if defined(_WIN32)
    char buf[MAX_PATH];
    DWORD len = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    path.assign(buf, len);
#else
    char buf[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf));
    if (len > 0) path.assign(buf, static_cast<std::size_t>(len));
#endif

    if (path.empty()) return ".";
    auto pos = path.find_last_of("/\\");
    return pos == std::string::npos ? "." : path.substr(0, pos);
}

// Same NEXUS_DB_PATH-first rule as persistence::detail::default_db_path()
// — both sides must agree so the auth DB and the row-store DB are always
// the same file.
inline std::string default_db_path() {
    if (const char* env = std::getenv("NEXUS_DB_PATH")) return env;
    return executable_dir() + "/nexus_data.db";
}

} // namespace nexus::authentication::detail
