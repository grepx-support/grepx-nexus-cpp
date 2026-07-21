cmake_minimum_required(VERSION 3.28)
project(nexus_server_app LANGUAGES CXX)
include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/NexusCommon.cmake)

nexus_add_dependencies(domain ports infra criteria application net protocol traits engine persistence session logging)
nexus_add_executable(nexus_server SOURCES src/main.cpp
    DEPENDS nexus_domain nexus_ports nexus_infra nexus_criteria nexus_application
            nexus_net nexus_protocol nexus_traits nexus_engine nexus_persistence nexus_session nexus_logging)
