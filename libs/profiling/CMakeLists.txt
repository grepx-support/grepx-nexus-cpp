cmake_minimum_required(VERSION 3.28)
project(nexus_profiling LANGUAGES CXX)
include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/NexusCommon.cmake)

# IMPORTANT: this library must never itself be compiled with
# -finstrument-functions, or __cyg_profile_func_enter/exit would call
# themselves and recurse forever. It is deliberately its own target so
# NexusCommon's per-target flags never touch it.
add_library(nexus_profiling STATIC src/profile_hooks.cpp)
target_compile_features(nexus_profiling PRIVATE cxx_std_23)
