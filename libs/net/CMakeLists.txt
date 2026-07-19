cmake_minimum_required(VERSION 3.28)
project(nexus_net LANGUAGES CXX)
include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/NexusCommon.cmake)

nexus_add_interface_library(nexus_net)

if(WIN32)
    target_link_libraries(nexus_net INTERFACE ws2_32)
else()
    target_link_libraries(nexus_net INTERFACE pthread)
endif()
