cmake_minimum_required(VERSION 3.28)
project(nexus_persistence LANGUAGES CXX)
include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/NexusCommon.cmake)

nexus_add_dependencies(domain ports traits)
nexus_add_interface_library(nexus_persistence DEPENDS nexus_domain nexus_ports nexus_traits)

if(NOT TARGET sqlite3)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../external/sqlite-amalgamation-3530300 ${CMAKE_CURRENT_BINARY_DIR}/_deps/sqlite3)
endif()

target_link_libraries(nexus_persistence INTERFACE sqlite3)
