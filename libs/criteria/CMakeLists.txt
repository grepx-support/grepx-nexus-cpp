# Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
# This file is part of the GrepX platform and is proprietary and confidential.
# Unauthorised copying, use, modification or distribution is strictly prohibited.

cmake_minimum_required(VERSION 3.28)
project(nexus_criteria LANGUAGES CXX)
include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/NexusCommon.cmake)

nexus_add_dependencies(domain ports)
nexus_add_interface_library(nexus_criteria DEPENDS nexus_domain nexus_ports)
