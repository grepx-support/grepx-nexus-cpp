cmake_minimum_required(VERSION 3.28)
project(nexus_demo_app LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

foreach(lib IN ITEMS domain ports infra criteria application traits engine)
    if(NOT TARGET nexus_${lib})
        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../libs/${lib} ${CMAKE_CURRENT_BINARY_DIR}/_deps/nexus_${lib})
    endif()
endforeach()

add_executable(nexus_demo src/main.cpp)
target_link_libraries(nexus_demo PRIVATE
    nexus_domain nexus_ports nexus_infra nexus_criteria nexus_application nexus_traits nexus_engine)
target_compile_options(nexus_demo PRIVATE -Wall -Wextra -Wpedantic)
