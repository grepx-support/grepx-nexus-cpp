# Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
# This file is part of the GrepX platform and is proprietary and confidential.
# Unauthorised copying, use, modification or distribution is strictly prohibited.

cmake_minimum_required(VERSION 3.28)
project(nexus_demo_app LANGUAGES CXX)
include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/NexusCommon.cmake)

nexus_add_dependencies(domain ports infra criteria application traits engine)
nexus_add_executable(nexus_demo SOURCES src/main.cpp
    DEPENDS nexus_domain nexus_ports nexus_infra nexus_criteria nexus_application nexus_traits nexus_engine)
