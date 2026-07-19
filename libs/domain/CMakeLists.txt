cmake_minimum_required(VERSION 3.28)
project(nexus_domain LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(nexus_domain INTERFACE)
target_include_directories(nexus_domain INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(nexus_domain INTERFACE cxx_std_23)

# Only build tests when this is the top-level project being configured
# (i.e. `cd libs/domain && cmake -B build`), not when pulled in as a
# dependency by another library's build.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    enable_testing()
    add_subdirectory(tests)
endif()
