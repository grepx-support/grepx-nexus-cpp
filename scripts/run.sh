#!/usr/bin/env bash
# Finds and starts the already-built nexus_server executable — saves
# hunting through cmake-build-debug/apps/nexus_server/ by hand every time.
#
# Doesn't build anything. If nothing is found, build the nexus_server
# CMake target first (CLion's Build button, or
# `cmake --build <build-dir> --target nexus_server`), then re-run this.
#
# All the detailed request/response logging (every REQUEST/RESPONSE line,
# nanosecond timestamps, login/logout, errors) lives inside nexus_server
# itself (see apps/nexus_server/src/main.cpp + libs/logging) — this
# script just launches whatever binary is already sitting on disk. If
# that binary predates a source change (this repo has had a lot of them:
# auth, sessions, logging), it won't have that code in it at all, and
# nothing here can fix that — only a rebuild can. The warning below is
# this script's only defense against silently launching a stale build.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."   # repo root

EXE=$(find . \( -path "*/cmake-build-*/apps/nexus_server/nexus_server" \
                 -o -path "*/cmake-build-*/apps/nexus_server/nexus_server.exe" \) \
              -type f 2>/dev/null | head -n 1)

if [ -z "$EXE" ]; then
    echo "run.sh: no built nexus_server found under any cmake-build-*/ directory." >&2
    echo "Build the nexus_server CMake target first, then re-run this." >&2
    exit 1
fi

# Warn if the binary is older than its own source — a stale build is
# exactly what produces "it started but nothing happens" confusion, and
# rebuilding is the only fix; this script can't do that itself (that's a
# CLion/CMake step, not a launch step).
SRC_DIR="apps/nexus_server/src"
if [ -d "$SRC_DIR" ]; then
    NEWEST_SRC=$(find "$SRC_DIR" -name '*.cpp' -o -name '*.hpp' 2>/dev/null | xargs -r ls -t 2>/dev/null | head -n 1)
    if [ -n "$NEWEST_SRC" ] && [ "$NEWEST_SRC" -nt "$EXE" ]; then
        echo "run.sh: WARNING — $EXE is older than $NEWEST_SRC." >&2
        echo "This binary predates a source change and may be missing recent" >&2
        echo "features (logging, auth, sessions, ...). Rebuild nexus_server" >&2
        echo "(CLion's Build button, or cmake --build <dir> --target nexus_server)" >&2
        echo "before relying on its output for debugging." >&2
        echo >&2
    fi
fi

# exit code 127 from a plain `"$EXE"` invocation under MSYS/Git-Bash
# means the OS loader could not resolve the executable at all — almost
# always a missing MinGW runtime DLL (libstdc++-6.dll, libgcc_s_seh-1.dll,
# libwinpthread-1.dll) or a missing dependency the exe was dynamically
# linked against (e.g. sqlite3.dll), NOT a transient AV lock (an AV lock
# reproduces as "No error"/exit 1-ish and is intermittent; 127 is a hard,
# consistent failure — retrying it 3 times just wastes 2 seconds).
# `ldd` (present in MSYS/Git-Bash) lists exactly which DLL is missing, so
# diagnose that up front instead of waiting for the retry loop to give up.
fix_missing_dlls() {
    if ! command -v ldd >/dev/null 2>&1; then
        # Git-for-Windows' bundled bash is a minimal MSYS runtime and
        # often doesn't ship ldd at all (that's the full MSYS2 install) —
        # silently giving up here left the user with zero diagnosis, so
        # hand them the one tool that's always available instead: Windows
        # itself. Double-clicking the exe (or running it from cmd.exe)
        # pops a dialog naming the exact missing DLL.
        echo "run.sh: can't auto-diagnose — 'ldd' isn't available in this" >&2
        echo "bash. To find the missing DLL by name, either:" >&2
        echo "  - double-click $EXE in File Explorer (Windows will show a" >&2
        echo "    dialog naming the missing DLL), or" >&2
        echo "  - run:  cmd //c \"$(cygpath -w "$EXE" 2>/dev/null || echo "$EXE")\"" >&2
        echo "Once you know the DLL name, either add the folder containing it" >&2
        echo "to PATH before running this script, or rebuild nexus_server" >&2
        echo "statically linked:" >&2
        echo "  target_link_options(nexus_server PRIVATE -static-libgcc -static-libstdc++ -static)" >&2
        return 1
    fi
    local missing
    missing=$(ldd "$EXE" 2>/dev/null | awk '/not found/ {print $1}')
    [ -z "$missing" ] && return 1

    echo "run.sh: missing DLL dependencies (this is why it's exiting 127):" >&2
    echo "$missing" | sed 's/^/  /' >&2

    local search_dirs=(
        /mingw64/bin /mingw32/bin /ucrt64/bin /clang64/bin
        /c/msys64/mingw64/bin /c/msys64/ucrt64/bin /c/msys64/clang64/bin
    )
    local dll d found_dir=""
    for dll in $missing; do
        for d in "${search_dirs[@]}"; do
            if [ -f "$d/$dll" ]; then
                found_dir="$d"
                break 2
            fi
        done
    done

    if [ -n "$found_dir" ]; then
        echo "run.sh: found a matching runtime at $found_dir — adding to PATH and retrying." >&2
        export PATH="$found_dir:$PATH"
        return 0
    fi

    echo "run.sh: couldn't find those DLLs in the usual MSYS2 toolchain" >&2
    echo "locations. Fix: add the bin/ directory that contains them (your" >&2
    echo "MinGW toolchain's — check CLion's Settings > Build, Execution," >&2
    echo "Deployment > Toolchains for its path) to PATH before running this" >&2
    echo "script, e.g.:" >&2
    echo "  export PATH=\"/path/to/mingw/bin:\$PATH\"" >&2
    echo "Or rebuild statically linked (add to nexus_server's CMakeLists.txt):" >&2
    echo "  target_link_options(nexus_server PRIVATE -static-libgcc -static-libstdc++ -static)" >&2
    return 1
}

echo "starting $EXE"

# Plain invocation, not `exec`: MSYS bash's exec doesn't reliably
# process-replace a native Windows PE binary the way it does a POSIX
# one, and a freshly-built .exe can transiently fail to launch on
# Windows (Defender/antivirus scanning a just-created executable is the
# usual cause) — that shows up here as a bogus-looking "No error".
# Retrying a couple of times with a short pause clears it without
# needing the user to just mash the up-arrow and re-run.
dll_fix_attempted=false
attempt=1
while true; do
    # NOT `if "$EXE"; then exit 0; fi; status=$?` — when the condition
    # of an if/then with no else is false, POSIX defines the *if
    # statement's own* exit status as 0, not the failing command's. So
    # `status=$?` right after that `fi` was always reading 0, no matter
    # what $EXE actually exited with (confirmed both on this machine and
    # in the field: "(exit 0)" printed on every failing attempt). Capture
    # the real status directly via `||` instead, which set -e also
    # leaves alone (it's not the final command in a checked list).
    status=0
    "$EXE" || status=$?
    if [ "$status" -eq 0 ]; then
        exit 0
    fi

    if [ "$status" -eq 127 ] && [ "$dll_fix_attempted" != true ]; then
        dll_fix_attempted=true
        if fix_missing_dlls; then
            echo "run.sh: retrying with fixed PATH..." >&2
            continue
        fi
    fi

    if [ "$attempt" -ge 3 ]; then
        echo "run.sh: $EXE failed to start after $attempt attempts (exit $status)." >&2
        if [ "$status" -eq 127 ]; then
            echo "Exit 127 means a missing DLL dependency, not an AV lock — see" >&2
            echo "the diagnosis above." >&2
        else
            echo "If this keeps happening, check Windows Defender / antivirus isn't" >&2
            echo "quarantining or locking a freshly-built .exe, and that nothing else" >&2
            echo "(another nexus_server, a locked DB file) is already holding it open." >&2
        fi
        exit "$status"
    fi
    echo "run.sh: launch attempt $attempt failed (exit $status), retrying..." >&2
    attempt=$((attempt + 1))
    sleep 1
done
