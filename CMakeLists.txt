cmake_minimum_required(VERSION 3.28)
project(nexus LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

enable_testing()

add_subdirectory(libs/domain)
add_subdirectory(libs/ports)
add_subdirectory(libs/infra)
add_subdirectory(libs/criteria)
add_subdirectory(libs/statemachine)
add_subdirectory(libs/application)
add_subdirectory(libs/traits)
add_subdirectory(libs/net)
add_subdirectory(libs/protocol)
add_subdirectory(libs/engine)
add_subdirectory(libs/persistence)
add_subdirectory(apps/nexus_demo)
add_subdirectory(apps/nexus_server)
add_subdirectory(apps/nexus_client)

# Aggregate every sub-project's tests here too, since the guard in each
# library's own CMakeLists.txt (CMAKE_SOURCE_DIR check) skips its tests
# when pulled in as a dependency of this top-level build.
foreach(lib IN ITEMS domain ports infra criteria application traits net protocol persistence statemachine)
    add_subdirectory(libs/${lib}/tests nexus_${lib}_tests_from_root)
endforeach()


