#pragma once
#include <chrono>
#include <cstdio>
#include <ctime>
#include <sstream>
#include <string>
#include <thread>

namespace nexus::logging::detail {

// "YYYY-MM-DD HH:MM:SS.nnnnnnnnn" — the nanosecond field is always 9
// digits wide so log lines sort/parse consistently regardless of the
// underlying clock's actual tick size (system_clock's real resolution
// is platform-dependent — often not true nanosecond on Windows — but
// the field width never changes, so downstream tooling can rely on it).
inline std::string now_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto since_epoch = now.time_since_epoch();
    auto secs = std::chrono::duration_cast<std::chrono::seconds>(since_epoch);
    auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(since_epoch - secs);

    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif

    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
                  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                  tm.tm_hour, tm.tm_min, tm.tm_sec);

    char full[48];
    std::snprintf(full, sizeof(full), "%s.%09lld", buf, static_cast<long long>(nanos.count()));
    return full;
}

// std::thread::id has no stable, short, printable form of its own —
// streaming it gives an implementation-defined (often long/opaque)
// representation, so route it through ostringstream once here rather
// than duplicating that at every logger. nexus_server is one thread per
// connection (see apps/nexus_server/src/main.cpp), so this is constant
// for every line a given connection logs — one more thing (besides
// conn=N) that ties a connection's lines together when several are
// interleaved in the same file.
inline std::string current_thread_id() {
    std::ostringstream oss;
    oss << std::this_thread::get_id();
    return oss.str();
}

} // namespace nexus::logging::detail
