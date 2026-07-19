foreach(test_name IN ITEMS select_service_test insert_service_test subscribe_service_test dispatch_service_test transition_service_test)
    nexus_add_test(nexus_application_${test_name} SOURCE ${test_name}.cpp LINK_LIBRARIES nexus_application)
endforeach()
