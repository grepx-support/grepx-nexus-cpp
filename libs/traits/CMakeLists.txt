cmake_minimum_required(VERSION 3.28)
project(nexus_traits LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

foreach(lib IN ITEMS domain ports infra criteria)
    if(NOT TARGET nexus_${lib})
        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../${lib} ${CMAKE_CURRENT_BINARY_DIR}/_deps/nexus_${lib})
    endif()
endforeach()

add_library(nexus_traits INTERFACE)
target_include_directories(nexus_traits INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(nexus_traits INTERFACE nexus_domain nexus_ports nexus_infra nexus_criteria)
target_compile_features(nexus_traits INTERFACE cxx_std_23)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    enable_testing()
    add_subdirectory(tests)
endif()
