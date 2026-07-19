foreach(test_name IN ITEMS row_test predicate_test)
    nexus_add_test(nexus_domain_${test_name} SOURCE ${test_name}.cpp LINK_LIBRARIES nexus_domain)
endforeach()
