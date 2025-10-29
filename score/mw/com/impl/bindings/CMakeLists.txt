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

# Add subdirectories for specific bindings
add_subdirectory(lola)
add_subdirectory(mock_binding)

# Bindings library - aggregates all binding implementations
add_library(com_impl_bindings INTERFACE)

target_include_directories(com_impl_bindings
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(com_impl_bindings
    INTERFACE
        com_impl_bindings_lola
        com_impl_bindings_mock
        message_passing
        score::score_language
)

# Add alias for exported target
add_library(score-communication::com_impl_bindings ALIAS com_impl_bindings)

# Install targets
install(TARGETS com_impl_bindings
    EXPORT score-communicationTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)