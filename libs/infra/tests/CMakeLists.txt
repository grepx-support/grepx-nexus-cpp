# Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
# This file is part of the GrepX platform and is proprietary and confidential.
# Unauthorised copying, use, modification or distribution is strictly prohibited.

foreach(test_name IN ITEMS snapshot_store_test audit_log_test subscription_registry_test subscription_registry_stress_test event_bus_test)
    nexus_add_test(nexus_infra_${test_name} SOURCE ${test_name}.cpp LINK_LIBRARIES nexus_infra)
endforeach()
