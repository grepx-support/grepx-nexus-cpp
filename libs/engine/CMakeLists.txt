cmake_minimum_required(VERSION 3.28)
project(nexus_engine LANGUAGES CXX)
include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/NexusCommon.cmake)

nexus_add_dependencies(domain ports application)
nexus_add_interface_library(nexus_engine DEPENDS nexus_domain nexus_ports nexus_application)
