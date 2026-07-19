cmake_minimum_required(VERSION 3.28)
project(nexus_infra LANGUAGES CXX)
include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/NexusCommon.cmake)

nexus_add_dependencies(domain ports)
nexus_add_interface_library(nexus_infra DEPENDS nexus_domain nexus_ports)
