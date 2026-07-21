cmake_minimum_required(VERSION 3.28)
project(nexus_traits LANGUAGES CXX)
include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/NexusCommon.cmake)

nexus_add_dependencies(domain ports infra criteria authentication session)
nexus_add_interface_library(nexus_traits DEPENDS nexus_domain nexus_ports nexus_infra nexus_criteria nexus_authentication nexus_session)
