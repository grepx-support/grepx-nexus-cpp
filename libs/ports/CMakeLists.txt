cmake_minimum_required(VERSION 3.28)
project(nexus_ports LANGUAGES CXX)
include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/NexusCommon.cmake)

nexus_add_dependency(domain)
nexus_add_interface_library(nexus_ports DEPENDS nexus_domain)
