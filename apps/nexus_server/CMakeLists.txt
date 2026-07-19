cmake_minimum_required(VERSION 3.28)
project(nexus_server_app LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

foreach (lib IN ITEMS domain ports infra criteria application net protocol traits engine persistence)
    if (NOT TARGET nexus_${lib})
        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../libs/${lib} ${CMAKE_CURRENT_BINARY_DIR}/_deps/nexus_${lib})
    endif ()
endforeach ()

add_executable(nexus_server src/main.cpp)
target_link_libraries(nexus_server PRIVATE
        nexus_domain nexus_ports nexus_infra nexus_criteria nexus_application
        nexus_net nexus_protocol nexus_traits nexus_engine nexus_persistence)
target_compile_options(nexus_server PRIVATE -Wall -Wextra -Wpedantic)
