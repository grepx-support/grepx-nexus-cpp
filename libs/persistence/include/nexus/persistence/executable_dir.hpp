#pragma once
#include <string>

// Isolates the one #ifdef _WIN32 branch this needs, same pattern as
// nexus/net/platform.hpp. Nothing outside this header should need to
// know how "where is my own .exe" is resolved per-platform.

#if defined(_WIN32)
    #include <windows.h>
#else
    #include <climits>
    #include <unistd.h>
#endif

namespace nexus::persistence {

// The directory containing the currently-running executable — NOT the
// process's current working directory, which is unreliable (varies by
// how the binary was launched: double-click, IDE run config with
// whatever working directory it defaults to, a terminal cd'd somewhere
// else...). Resolving against the exe's own location instead means
// "where does nexus_server put its DB" has exactly one answer no
// matter how you start it.
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

} // namespace nexus::persistence
