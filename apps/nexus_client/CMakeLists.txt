# Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
# This file is part of the GrepX platform and is proprietary and confidential.
# Unauthorised copying, use, modification or distribution is strictly prohibited.

cmake_minimum_required(VERSION 3.28)
project(nexus_client_app LANGUAGES CXX)
include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/NexusCommon.cmake)

nexus_add_dependencies(domain net protocol)

set(NEXUS_CLIENT_DEPS nexus_domain nexus_net nexus_protocol)

nexus_add_executable(nexus_client SOURCES src/main.cpp DEPENDS ${NEXUS_CLIENT_DEPS})
nexus_add_executable(nexus_insert SOURCES src/insert_client.cpp DEPENDS ${NEXUS_CLIENT_DEPS})
nexus_add_executable(nexus_notify_a SOURCES src/notify_client_a.cpp DEPENDS ${NEXUS_CLIENT_DEPS})
nexus_add_executable(nexus_notify_b SOURCES src/notify_client_b.cpp DEPENDS ${NEXUS_CLIENT_DEPS})
