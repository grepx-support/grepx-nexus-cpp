cmake_minimum_required(VERSION 3.28)
project(nexus_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

foreach(lib IN ITEMS domain ports application)
    if(NOT TARGET nexus_${lib})
        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../${lib} ${CMAKE_CURRENT_BINARY_DIR}/_deps/nexus_${lib})
    endif()
endforeach()

add_library(nexus_engine INTERFACE)
target_include_directories(nexus_engine INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(nexus_engine INTERFACE nexus_domain nexus_ports nexus_application)
target_compile_features(nexus_engine INTERFACE cxx_std_23)
