cmake_minimum_required(VERSION 3.28)
project(nexus_application LANGUAGES CXX)
include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/NexusCommon.cmake)

nexus_add_dependencies(domain ports statemachine)

# Deliberately NOT linked here: nexus_infra, nexus_criteria.
# Tests use local fakes to prove application has no compiled dependency
# on any concrete adapter.
nexus_add_interface_library(nexus_application DEPENDS nexus_domain nexus_ports nexus_statemachine)
