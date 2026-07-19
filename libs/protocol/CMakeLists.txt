cmake_minimum_required(VERSION 3.28)
project(nexus_protocol LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT TARGET nexus_domain)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../domain ${CMAKE_CURRENT_BINARY_DIR}/_deps/nexus_domain)
endif()

add_library(nexus_protocol INTERFACE)
target_include_directories(nexus_protocol INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(nexus_protocol INTERFACE nexus_domain)
target_compile_features(nexus_protocol INTERFACE cxx_std_23)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    enable_testing()
    add_subdirectory(tests)
endif()
