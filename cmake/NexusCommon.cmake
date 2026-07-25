# Shared settings + helper functions for every Nexus CMakeLists.txt.
#
# Every libs/*/CMakeLists.txt and apps/*/CMakeLists.txt used to hand-copy
# the same handful of blocks: the C++23 standard setup, the -Wall/-Wextra/
# -Wpedantic flags, the "if(NOT TARGET nexus_X) add_subdirectory(...)"
# dependency pattern, and the add_executable/target_link_libraries/
# add_test boilerplate for a unit test. That's all centralized here —
# include() this once per CMakeLists.txt, right after your own
# cmake_minimum_required()/project() call (those two stay per-file: each
# library is still independently buildable standalone, e.g.
# `cd libs/domain && cmake -B build`, which needs its own project()).
#
# Usage from libs/<name>/CMakeLists.txt:
#     include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/NexusCommon.cmake)
# Usage from libs/<name>/tests/CMakeLists.txt:
#     include(${CMAKE_CURRENT_LIST_DIR}/../../../cmake/NexusCommon.cmake)
# Usage from apps/<name>/CMakeLists.txt:
#     include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/NexusCommon.cmake)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(NEXUS_WARNING_FLAGS -Wall -Wextra -Wpedantic)

# Resolved once, from this file's own fixed location — correct whether
# it's included from a from-root build or a standalone single-library
# build, since it never depends on CMAKE_SOURCE_DIR.
get_filename_component(NEXUS_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(NEXUS_LIBS_DIR "${NEXUS_ROOT_DIR}/libs")

# nexus_add_dependency(<lib_name>)
#   Wraps "if(NOT TARGET nexus_<lib_name>) add_subdirectory(libs/<lib_name> ...) endif()"
#   — the pattern every module used to hand-write per dependency.
function(nexus_add_dependency lib_name)
    if(NOT TARGET nexus_${lib_name})
        add_subdirectory(
                "${NEXUS_LIBS_DIR}/${lib_name}"
                "${CMAKE_CURRENT_BINARY_DIR}/_deps/nexus_${lib_name}"
        )
    endif()
endfunction()

# nexus_add_dependencies(<lib_name>...)
#   Same, for a whole list at once — replaces the foreach(lib IN ITEMS ...) blocks.
function(nexus_add_dependencies)
    foreach(lib_name IN LISTS ARGN)
        nexus_add_dependency(${lib_name})
    endforeach()
endfunction()

# nexus_add_interface_library(<target> [DEPENDS <lib>...])
#   Declares <target> as a header-only INTERFACE library exposing its
#   own include/ dir, applies C++23, links DEPENDS, and — only when this
#   module is the top-level project being configured (i.e. built
#   standalone, not pulled in via add_subdirectory) — enables testing
#   and adds its tests/ subdirectory.
function(nexus_add_interface_library target)
    cmake_parse_arguments(ARG "" "" "DEPENDS" ${ARGN})

    add_library(${target} INTERFACE)
    target_include_directories(${target} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_compile_features(${target} INTERFACE cxx_std_23)
    if(ARG_DEPENDS)
        target_link_libraries(${target} INTERFACE ${ARG_DEPENDS})
    endif()

    if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
        enable_testing()
        # Not every module has a tests/ dir (e.g. libs/engine is pure
        # composition, nothing to unit test on its own) — only descend
        # into it if it actually exists.
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
            add_subdirectory(tests)
        endif()
    endif()
endfunction()

# nexus_add_executable(<target> SOURCES <src>... [DEPENDS <lib>...])
#   Declares an application executable with the standard warning flags.
function(nexus_add_executable target)
    cmake_parse_arguments(ARG "" "" "SOURCES;DEPENDS" ${ARGN})
    add_executable(${target} ${ARG_SOURCES})
    if(ARG_DEPENDS)
        target_link_libraries(${target} PRIVATE ${ARG_DEPENDS})
    endif()
    target_compile_options(${target} PRIVATE ${NEXUS_WARNING_FLAGS})

    # On MinGW, the .exe dynamically links libstdc++-6.dll,
    # libgcc_s_seh-1.dll, and libwinpthread-1.dll by default. CLion's own
    # run/debug puts the toolchain's bin/ on PATH so this is invisible
    # there, but launching the same .exe from an arbitrary Git-Bash or
    # cmd.exe session (nothing else adds that directory to PATH) fails
    # outright — the OS loader can't resolve those DLLs and the process
    # never starts (surfaces as exit code 127, no other diagnostic).
    # Statically linking them in makes the exe self-contained: it runs
    # from any shell, no PATH setup required, no DLLs to ship alongside
    # it.
    if(MINGW)
        # -static-libstdc++ alone (even without a blanket -static) still
        # collides here: "multiple definition of
        # `std::type_info::operator==`" between libstdc++.a's tinfo.o and
        # the inline weak definition GCC emits from <typeinfo> into any
        # TU using RTTI comparisons (std::visit, dynamic_cast, catch
        # blocks — main.cpp uses all three). Confirmed reproducible on
        # this exact toolchain (CLion's bundled mingw, GCC 13.1.0) with
        # nothing else in the flags changed — this is a packaging bug in
        # that libstdc++.a, not something specific to nexus_server.
        # Both definitions are the same weak symbol from libstdc++
        # itself, so telling the linker to keep the first one and move on
        # (--allow-multiple-definition) is safe, not a functional
        # workaround-of-a-workaround.
        target_link_options(${target} PRIVATE
                -static-libgcc -static-libstdc++
                -Wl,--allow-multiple-definition)

        # --whole-archive on libwinpthread.a forces in its TLS-callback
        # registration object (needed so thread_local destructors run
        # when winpthread is statically linked). On GCC 13.1.0 (CLion's
        # older bundled mingw) that was safe. On GCC 15.2.0 (current
        # bundled mingw as of CLion 2026.2), whole-archiving also drags
        # in winpthread's cancellation-cleanup objects, which reference
        # `__intrinsic_setjmpex` — a symbol that isn't resolvable against
        # this build's libgcc, producing an undefined-reference link
        # failure. That's a libwinpthread.a/libgcc version mismatch in
        # the bundled toolchain, not something to fix in application
        # code. Until CLion ships a matched pair (or we pin an MSYS2
        # toolchain instead), skip the whole-archive trick on newer GCC
        # and fall back to an ordinary static link of winpthread; if
        # thread_local destructors turn out not to run in that
        # configuration, revisit with a narrower fix that whole-archives
        # only the specific TLS-callback object rather than the whole
        # archive.
        if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 14.0)
            target_link_options(${target} PRIVATE
                    -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive,-Bdynamic)
        else()
            target_link_options(${target} PRIVATE
                    -Wl,-Bstatic -lwinpthread -Wl,-Bdynamic)
        endif()
    endif()
endfunction()

# nexus_add_test(<target> [SOURCE <file.cpp>] LINK_LIBRARIES <lib>...)
#   Declares one test executable, applies warning flags, registers it
#   with ctest. SOURCE defaults to "<target>.cpp" if omitted.
function(nexus_add_test target)
    cmake_parse_arguments(ARG "" "SOURCE" "LINK_LIBRARIES" ${ARGN})
    if(NOT ARG_SOURCE)
        set(ARG_SOURCE "${target}.cpp")
    endif()

    add_executable(${target} ${ARG_SOURCE})
    if(ARG_LINK_LIBRARIES)
        target_link_libraries(${target} PRIVATE ${ARG_LINK_LIBRARIES})
    endif()
    target_compile_options(${target} PRIVATE ${NEXUS_WARNING_FLAGS})
    add_test(NAME ${target} COMMAND ${target})
endfunction()