# *******************************************************************************
# Copyright (c) 2025 Contributors to the Eclipse Foundation
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

# LOLA messaging messages implementation
add_library(com_impl_bindings_lola_messaging_messages
    message_common.cpp
    message_element_fq_id.cpp
    message_outdated_nodeid.cpp
)

target_include_directories(com_impl_bindings_lola_messaging_messages
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_sources(com_impl_bindings_lola_messaging_messages
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${CMAKE_SOURCE_DIR}/score/mw/com/impl/bindings/lola/messaging/messages
        FILES
            message_common.h
            message_element_fq_id.h
            message_outdated_nodeid.h
)

target_link_libraries(com_impl_bindings_lola_messaging_messages
    PUBLIC
        message_passing
        score::score_containers
        score::score_language
)

# Install targets
install(TARGETS com_impl_bindings_lola_messaging_messages
    EXPORT score-communicationTargets
    FILE_SET HEADERS DESTINATION include/score/mw/com/impl/bindings/lola/messaging/messages
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)