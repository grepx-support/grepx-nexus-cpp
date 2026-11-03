# Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
# This file is part of the GrepX platform and is proprietary and confidential.
# Unauthorised copying, use, modification or distribution is strictly prohibited.

cmake_minimum_required(VERSION 3.28)
project(nexus_persistence_fast LANGUAGES CXX)
include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/NexusCommon.cmake)

nexus_add_dependencies(domain ports traits persistence)
nexus_add_interface_library(nexus_persistence_fast
    DEPENDS nexus_domain nexus_ports nexus_traits nexus_persistence)

if(NOT TARGET sqlite3)
    add_subdirectory(
        ${CMAKE_CURRENT_SOURCE_DIR}/../../external/sqlite-amalgamation-3530300
        ${CMAKE_CURRENT_BINARY_DIR}/_deps/sqlite3)
endif()

target_link_libraries(nexus_persistence_fast INTERFACE sqlite3)
