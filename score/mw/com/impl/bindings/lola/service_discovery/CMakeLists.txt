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

# Add subdirectories
add_subdirectory(client)
add_subdirectory(test)

# LOLA service discovery implementation
add_library(com_impl_bindings_lola_service_discovery
    flag_file.cpp
    flag_file_crawler.cpp
    known_instances_container.cpp
    lola_service_instance_identifier.cpp
    quality_aware_container.cpp
)

target_include_directories(com_impl_bindings_lola_service_discovery
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_sources(com_impl_bindings_lola_service_discovery
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${CMAKE_SOURCE_DIR}/score/mw/com/impl/bindings/lola/service_discovery
        FILES
            flag_file.h
            flag_file_crawler.h
            known_instances_container.h
            lola_service_instance_identifier.h
            quality_aware_container.h
)

target_link_libraries(com_impl_bindings_lola_service_discovery
    PUBLIC
        score::score_filesystem
        score::score_filesystem_i_standard
        score::score_os
        score::score_result
        score::score_safecpp
        score::score_futurecpp
        score::score_concurrency
        score::score_memory
        score::score_mw_log_core
        com_impl_core
)

# Install targets
install(TARGETS com_impl_bindings_lola_service_discovery
    EXPORT score-communicationTargets
    FILE_SET HEADERS DESTINATION include/score/mw/com/impl/bindings/lola/service_discovery
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
