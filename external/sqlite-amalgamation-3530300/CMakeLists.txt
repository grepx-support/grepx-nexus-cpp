cmake_minimum_required(VERSION 3.28)
project(sqlite3 C)

add_library(sqlite3 STATIC sqlite3.c)
target_include_directories(sqlite3 PUBLIC .)

if(WIN32)
    target_compile_definitions(sqlite3 PRIVATE SQLITE_OS_WIN=1)
elseif(UNIX)
    target_compile_definitions(sqlite3 PRIVATE SQLITE_OS_UNIX=1)
endif()