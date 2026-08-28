// GCC/Clang -finstrument-functions hooks.
// Built WITHOUT -finstrument-functions itself (excluded via
// -fno-instrument-functions on this one file in CMake) to avoid infinite
// recursion on entry/exit of these two functions.
//
// Every instrumented function's entry/exit is timestamped with
// std::chrono::steady_clock (nanosecond resolution on this toolchain).
// Per (function address) stats -- call count, total/min/max nanoseconds --
// accumulate in a flat in-memory table (no I/O on the hot path).
//
// This process is long-running, so the table is NOT only dumped at exit:
//   1. A background thread rewrites nexus_profile.csv every N seconds
//      (see PERIODIC_DUMP_INTERVAL_SECONDS) -- `cat`/`tail` it anytime.
//   2. On-demand snapshot, right after firing a specific request:
//        - Linux/macOS: kill -SIGUSR1 $(pgrep nexus_server)
//        - Windows: create an empty file named "dump_now" next to where
//          the process is running (e.g. `type nul > dump_now` in cmd, or
//          `New-Item dump_now` in PowerShell) -- the background thread
//          notices it, dumps, and deletes the trigger file.
//   3. A normal clean exit (main() returns / exit() called) also dumps,
//      via std::atexit.
//
// Addresses are resolved to source names offline via
// scripts/resolve_profile.py + addr2line (needs the .exe built with -g).

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#if !defined(_WIN32)
#include <csignal>
#define NEXUS_HAVE_POSIX_SIGNALS 1
#endif

extern "C" {

struct FuncStats {
    uint64_t calls = 0;
    uint64_t total_ns = 0;
    uint64_t min_ns = UINT64_MAX;
    uint64_t max_ns = 0;
};

namespace {

// How often the background thread rewrites the CSV on its own, and (on
// Windows) how often it checks for the "dump_now" trigger file.
constexpr int PERIODIC_DUMP_INTERVAL_SECONDS = 10;
constexpr int WINDOWS_TRIGGER_POLL_MS = 500;
const char* TRIGGER_FILE = "dump_now";

std::mutex g_mutex;
std::unordered_map<void*, FuncStats> g_stats;

// Per-thread call stack of entry timestamps.
thread_local std::vector<std::chrono::steady_clock::time_point> t_enter_stack;

// Guards one-time setup so it only happens once no matter how many
// threads/functions call __cyg_profile_func_enter first.
std::atomic<bool> g_setup_done{false};

void dump_snapshot() {
    std::lock_guard<std::mutex> lock(g_mutex);
    std::FILE* f = std::fopen("nexus_profile.csv", "w");
    if (!f) return;
    std::fprintf(f, "address,calls,total_ns,avg_ns,min_ns,max_ns\n");
    for (auto& [addr, s] : g_stats) {
        if (s.calls == 0) continue;
        uint64_t avg = s.total_ns / s.calls;
        std::fprintf(f, "%p,%llu,%llu,%llu,%llu,%llu\n",
                      addr,
                      (unsigned long long)s.calls,
                      (unsigned long long)s.total_ns,
                      (unsigned long long)avg,
                      (unsigned long long)s.min_ns,
                      (unsigned long long)s.max_ns);
    }
    std::fclose(f);
}

// Clean process exit (main() returns, or exit() is called).
void dump_on_exit() {
    dump_snapshot();
}

#if defined(NEXUS_HAVE_POSIX_SIGNALS)
// Linux/macOS: "kill -SIGUSR1 <pid>" -- dump right now, process keeps running.
void on_demand_dump(int /*sig*/) {
    dump_snapshot();
}
#endif

bool trigger_file_exists() {
    std::FILE* f = std::fopen(TRIGGER_FILE, "r");
    if (!f) return false;
    std::fclose(f);
    return true;
}

// Background thread:
//   - always: rewrite the CSV every PERIODIC_DUMP_INTERVAL_SECONDS.
//   - on Windows only: also poll for the "dump_now" trigger file every
//     WINDOWS_TRIGGER_POLL_MS and dump immediately (then delete it) when
//     it appears, since Windows has no SIGUSR1 to signal with.
void periodic_dump_loop() {
#if defined(NEXUS_HAVE_POSIX_SIGNALS)
    for (;;) {
        std::this_thread::sleep_for(std::chrono::seconds(PERIODIC_DUMP_INTERVAL_SECONDS));
        dump_snapshot();
    }
#else
    using namespace std::chrono;
    auto next_periodic = steady_clock::now() + seconds(PERIODIC_DUMP_INTERVAL_SECONDS);
    for (;;) {
        std::this_thread::sleep_for(milliseconds(WINDOWS_TRIGGER_POLL_MS));
        if (trigger_file_exists()) {
            dump_snapshot();
            std::remove(TRIGGER_FILE);
        }
        if (steady_clock::now() >= next_periodic) {
            dump_snapshot();
            next_periodic = steady_clock::now() + seconds(PERIODIC_DUMP_INTERVAL_SECONDS);
        }
    }
#endif
}

void one_time_setup() {
    std::atexit(dump_on_exit);
#if defined(NEXUS_HAVE_POSIX_SIGNALS)
    std::signal(SIGUSR1, on_demand_dump);
#endif
    std::thread(periodic_dump_loop).detach();
}

} // namespace

// Called at the start of every instrumented function.
void __cyg_profile_func_enter(void* this_fn, void* /*call_site*/) {
    (void)this_fn;
    if (!g_setup_done.exchange(true)) {
        one_time_setup();
    }
    t_enter_stack.push_back(std::chrono::steady_clock::now());
}

// Called at the end of every instrumented function.
void __cyg_profile_func_exit(void* this_fn, void* /*call_site*/) {
    if (t_enter_stack.empty()) return;
    auto t0 = t_enter_stack.back();
    t_enter_stack.pop_back();
    uint64_t elapsed_ns = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - t0).count());

    std::lock_guard<std::mutex> lock(g_mutex);
    FuncStats& s = g_stats[this_fn];
    s.calls++;
    s.total_ns += elapsed_ns;
    if (elapsed_ns < s.min_ns) s.min_ns = elapsed_ns;
    if (elapsed_ns > s.max_ns) s.max_ns = elapsed_ns;
}

} // extern "C"
