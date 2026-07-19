cmake_minimum_required(VERSION 3.28)
project(nexus_statemachine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(nexus_statemachine INTERFACE)
target_include_directories(nexus_statemachine INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(nexus_statemachine INTERFACE cxx_std_23)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    enable_testing()
    add_subdirectory(tests)
endif()