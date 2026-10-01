# Copyright (C) 2015-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


set(EXAMPLE_CONFIG_FILES "")

# Examples
add_executable(request-sample request-sample.cpp)
lgsomeip_configure_target(request-sample)
target_link_libraries(request-sample vsomeip-if)

add_executable(response-sample response-sample.cpp)
lgsomeip_configure_target(response-sample)
target_link_libraries(response-sample vsomeip-if)

add_executable(subscribe-sample subscribe-sample.cpp)
lgsomeip_configure_target(subscribe-sample)
target_link_libraries(subscribe-sample vsomeip-if)

add_executable(notify-sample notify-sample.cpp)
lgsomeip_configure_target(notify-sample)
target_link_libraries(notify-sample vsomeip-if)

#add_dependencies(examples request-sample response-sample subscribe-sample notify-sample)

#install (
#    TARGETS request-sample response-sample subscribe-sample notify-sample
#    RUNTIME DESTINATION "${INSTALL_BIN_DIR}" COMPONENT bin
#)

###################################################################################################
