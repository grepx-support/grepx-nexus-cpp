cmake_minimum_required(VERSION 3.28)
project(nexus_application LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT TARGET nexus_domain)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../domain ${CMAKE_CURRENT_BINARY_DIR}/_deps/nexus_domain)
endif ()
if (NOT TARGET nexus_ports)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../ports ${CMAKE_CURRENT_BINARY_DIR}/_deps/nexus_ports)
endif ()

if (NOT TARGET nexus_statemachine)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../statemachine ${CMAKE_CURRENT_BINARY_DIR}/_deps/nexus_statemachine)
endif ()

add_library(nexus_application INTERFACE)

target_include_directories(nexus_application INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(nexus_application INTERFACE nexus_domain nexus_ports nexus_statemachine)
target_compile_features(nexus_application INTERFACE cxx_std_23)

# Deliberately NOT linked here: nexus_infra, nexus_criteria.
# Tests below use local fakes to prove application has no compiled
# dependency on any concrete adapter.
if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    enable_testing()
    add_subdirectory(tests)
endif ()
