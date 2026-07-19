cmake_minimum_required(VERSION 3.28)
project(nexus_persistence LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

foreach(lib IN ITEMS domain ports traits)
    if(NOT TARGET nexus_${lib})
        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../${lib} ${CMAKE_CURRENT_BINARY_DIR}/_deps/nexus_${lib})
    endif()
endforeach()

add_library(nexus_persistence INTERFACE)
target_include_directories(nexus_persistence INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(nexus_persistence INTERFACE nexus_domain nexus_ports nexus_traits)
target_compile_features(nexus_persistence INTERFACE cxx_std_23)


if(NOT TARGET sqlite3)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../external/sqlite-amalgamation-3530300 ${CMAKE_CURRENT_BINARY_DIR}/_deps/sqlite3)
endif()

target_link_libraries(nexus_persistence INTERFACE sqlite3)