# Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
# This file is part of the GrepX platform and is proprietary and confidential.
# Unauthorised copying, use, modification or distribution is strictly prohibited.

cmake_minimum_required(VERSION 3.28)
project(nexus_session LANGUAGES CXX)
include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/NexusCommon.cmake)

# SqliteLoginAudit reuses nexus_authentication's db_path.hpp (same DB,
# same NEXUS_DB_PATH-first resolution) and gets sqlite3 transitively
# through it — no separate sqlite3 add_subdirectory needed here.
nexus_add_dependencies(authentication)
nexus_add_interface_library(nexus_session DEPENDS nexus_authentication)
