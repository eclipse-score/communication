# *******************************************************************************
# Copyright (C) 2017-2026 LG Electronics Inc.
#
# See the NOTICE file(s) distributed with this work for additional
# information regarding copyright ownership.
#
# This program and the accompanying materials are made available under the
# terms of the Apache License Version 2.0 which is available at
# https://www.apache.org/licenses/LICENSE-2.0
#
# SPDX-License-Identifier: Apache-2.0
# *******************************************************************************

file(GLOB_RECURSE SOMEIP_INTEGRATION_TEST_SOURCE CONFIGURE_DEPENDS
    runtime/*.cpp
)

add_executable(unittest_someip_integration ${SOMEIP_INTEGRATION_TEST_SOURCE})
someip_configure_target(unittest_someip_integration)
target_compile_definitions(unittest_someip_integration PRIVATE
    LGSOMEIP_TEST_CONFIG_DIR="${CMAKE_CURRENT_BINARY_DIR}"
)
target_link_libraries(unittest_someip_integration
    PRIVATE
        lgsomeip::lgsomeip-daemon-core
        lgsomeip::test-dependencies
        ${ADDITIONAL_LIBS}
)

add_test(NAME someip.integration COMMAND unittest_someip_integration)
set_tests_properties(someip.integration PROPERTIES
    LABELS integration
    TIMEOUT 180
)

file(COPY
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime/someip_config.json
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime/someip_config.json_error
    DESTINATION ${CMAKE_CURRENT_BINARY_DIR}
)
