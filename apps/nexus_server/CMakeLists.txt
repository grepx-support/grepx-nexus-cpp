cmake_minimum_required(VERSION 3.28)
project(nexus_server_app LANGUAGES CXX)
include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/NexusCommon.cmake)

nexus_add_dependencies(domain ports infra criteria application net protocol traits engine persistence session logging)
nexus_add_executable(nexus_server SOURCES src/main.cpp
    DEPENDS nexus_domain nexus_ports nexus_infra nexus_criteria nexus_application
            nexus_net nexus_protocol nexus_traits nexus_engine nexus_persistence nexus_session nexus_logging)

# See top-level CMakeLists.txt (NEXUS_ENABLE_PROFILING) and libs/profiling.
# Adds -finstrument-functions to this target only + links the hook impl —
# no source file in this app is modified.
if(NEXUS_ENABLE_PROFILING)
    target_compile_options(nexus_server PRIVATE -finstrument-functions -g)
    target_link_libraries(nexus_server PRIVATE nexus_profiling)
endif()

# Ultra-low-latency in-memory store with async WAL checkpoint.
# Enable with:  cmake -B cmake-build-profile -DNEXUS_FAST_STORE=ON
# Disable with: cmake -B cmake-build-profile -DNEXUS_FAST_STORE=OFF  (default)
if(NEXUS_FAST_STORE)
    target_compile_definitions(nexus_server PRIVATE NEXUS_USE_FAST_STORE=1)
    target_link_libraries(nexus_server PRIVATE nexus_persistence_fast)
endif()
