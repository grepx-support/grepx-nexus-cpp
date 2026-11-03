# Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
# This file is part of the GrepX platform and is proprietary and confidential.
# Unauthorised copying, use, modification or distribution is strictly prohibited.

cmake_minimum_required(VERSION 3.28)
project(nexus_authentication LANGUAGES CXX)
include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/NexusCommon.cmake)

nexus_add_interface_library(nexus_authentication)

# DbAuthenticator talks to sqlite directly (the vendored amalgamation) —
# deliberately NOT nexus_persistence: persistence depends on nexus_traits,
# and nexus_traits needs nexus_authentication for AuthTraits, so linking
# against nexus_persistence here would create a dependency cycle.
if (NOT TARGET sqlite3)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../external/sqlite-amalgamation-3530300 ${CMAKE_CURRENT_BINARY_DIR}/_deps/sqlite3)
endif ()
target_link_libraries(nexus_authentication INTERFACE sqlite3)
