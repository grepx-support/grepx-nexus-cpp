cmake_minimum_required(VERSION 3.28)
project(nexus_client_app LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

foreach(lib IN ITEMS domain net protocol)
    if(NOT TARGET nexus_${lib})
        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../libs/${lib} ${CMAKE_CURRENT_BINARY_DIR}/_deps/nexus_${lib})
    endif()
endforeach()

add_executable(nexus_client src/main.cpp)
target_link_libraries(nexus_client PRIVATE nexus_domain nexus_net nexus_protocol)
target_compile_options(nexus_client PRIVATE -Wall -Wextra -Wpedantic)

add_executable(nexus_insert src/insert_client.cpp)
target_link_libraries(nexus_insert PRIVATE nexus_domain nexus_net nexus_protocol)

add_executable(nexus_notify_a src/notify_client_a.cpp)
target_link_libraries(nexus_notify_a PRIVATE nexus_domain nexus_net nexus_protocol)

add_executable(nexus_notify_b src/notify_client_b.cpp)
target_link_libraries(nexus_notify_b PRIVATE nexus_domain nexus_net nexus_protocol)
