cmake_minimum_required(VERSION 3.28)
project(nexus_net LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(nexus_net INTERFACE)
target_include_directories(nexus_net INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(nexus_net INTERFACE cxx_std_23)

if(WIN32)
    target_link_libraries(nexus_net INTERFACE ws2_32)
else()
    target_link_libraries(nexus_net INTERFACE pthread)
endif()

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    enable_testing()
    add_subdirectory(tests)
endif()
