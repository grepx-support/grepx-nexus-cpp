# Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
# This file is part of the GrepX platform and is proprietary and confidential.
# Unauthorised copying, use, modification or distribution is strictly prohibited.

cmake_minimum_required(VERSION 3.28)
project(nexus LANGUAGES CXX)

include(${CMAKE_CURRENT_LIST_DIR}/cmake/NexusCommon.cmake)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Per-function latency profiling (see libs/profiling). Off by default —
# zero effect on normal builds. Turn on with:
#   cmake -B cmake-build-debug -DNEXUS_ENABLE_PROFILING=ON
#option(NEXUS_ENABLE_PROFILING "Instrument every function with entry/exit nanosecond timing" OFF)

# Ultra-low-latency in-memory SQLite store with async WAL checkpoint to disk.
# When ON, nexus_server is compiled with NEXUS_USE_FAST_STORE=1 and uses
# SqliteInMemStore instead of SqliteSnapshotStore — microsecond write
# latency on the hot path, checkpoint to disk every 500 ms by default.
# When OFF (default), the original file-backed SqliteSnapshotStore is used.
#
# Switch:  cmake -B cmake-build-profile -DNEXUS_FAST_STORE=ON
#          cmake -B cmake-build-profile -DNEXUS_FAST_STORE=OFF
option(NEXUS_FAST_STORE "Use in-memory SQLite with async WAL checkpoint (ultra-low latency)" OFF)

enable_testing()

#if(NEXUS_ENABLE_PROFILING)
#    add_subdirectory(libs/profiling)
#endif()

add_subdirectory(libs/domain)
add_subdirectory(libs/ports)
add_subdirectory(libs/infra)
add_subdirectory(libs/criteria)
add_subdirectory(libs/statemachine)
add_subdirectory(libs/application)
add_subdirectory(libs/logging)
add_subdirectory(libs/authentication)
add_subdirectory(libs/session)
add_subdirectory(libs/traits)
add_subdirectory(libs/net)
add_subdirectory(libs/protocol)
add_subdirectory(libs/engine)
add_subdirectory(libs/persistence)
add_subdirectory(libs/persistence_fast)   # always built; linked only when NEXUS_FAST_STORE=ON
# Fetches and builds protobuf from source on first use — a genuinely
# slow, higher-risk build (see libs/proto/CMakeLists.txt's comment).
# Opt in by uncommenting once you're ready to pay that cost; nothing
# currently on the wire depends on it.
#add_subdirectory(libs/proto)
#add_subdirectory(apps/nexus_demo)
add_subdirectory(apps/nexus_server)
#add_subdirectory(apps/nexus_client)

# Aggregate every sub-project's tests here too, since the guard in each
# library's own CMakeLists.txt (CMAKE_SOURCE_DIR check) skips its tests
# when pulled in as a dependency of this top-level build.
foreach(lib IN ITEMS domain ports infra criteria application traits net protocol persistence statemachine)
    add_subdirectory(libs/${lib}/tests nexus_${lib}_tests_from_root)
endforeach()

# Convenience: `cmake --build <build-dir> --target nexus_all_apps` builds
# every runnable executable in one shot. A plain full build (no --target,
# or CLion's "All CTest"/"All targets" configuration) already builds
# everything below by default — this target exists for when you only
# want the apps, not every test binary too.
add_custom_target(nexus_all_apps DEPENDS
    nexus_server
    nexus_demo
    nexus_client
    nexus_insert
    nexus_notify_a
    nexus_notify_b
)
