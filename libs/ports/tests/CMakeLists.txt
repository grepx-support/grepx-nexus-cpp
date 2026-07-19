add_executable(nexus_ports_tests concepts_test.cpp)
target_link_libraries(nexus_ports_tests PRIVATE nexus_ports)
target_compile_options(nexus_ports_tests PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME nexus_ports_tests COMMAND nexus_ports_tests)
