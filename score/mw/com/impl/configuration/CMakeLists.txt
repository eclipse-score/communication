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

# Configuration library - handles JSON configuration parsing
add_library(com_impl_configuration
    service_identifier_type.cpp
    service_instance_id.cpp
    service_instance_deployment.cpp
    service_type_deployment.cpp
    lola_service_instance_id.cpp
    lola_service_instance_deployment.cpp
    lola_service_type_deployment.cpp
    binding_service_type_deployment.cpp
    configuration.cpp
    config_parser.cpp
    configuration_error.cpp
    quality_type.cpp
    service_version_type.cpp
    someip_service_instance_id.cpp
    someip_service_instance_deployment.cpp
    lola_event_id.cpp
    lola_event_instance_deployment.cpp
    lola_field_id.cpp
    lola_field_instance_deployment.cpp
    someip_event_instance_deployment.cpp
    someip_field_instance_deployment.cpp
    shm_size_calc_mode.cpp
    tracing_configuration.cpp
    global_configuration.cpp
    configuration_common_resources.cpp
)

target_include_directories(com_impl_configuration
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

# Add header files
target_sources(com_impl_configuration
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${CMAKE_SOURCE_DIR}/score/mw/com/impl/configuration
        FILES
            lola_service_id.h
            lola_service_type_deployment.h
            binding_service_type_deployment.tpp
            service_identifier_type.h
            service_instance_id.h
            service_instance_deployment.h
            service_type_deployment.h
            lola_service_instance_id.h
            lola_service_instance_deployment.h
            lola_service_type_deployment.h
            binding_service_type_deployment.h
            configuration.h
            config_parser.h
            configuration_error.h
            quality_type.h
            service_version_type.h
            someip_service_instance_id.h
            someip_service_instance_deployment.h
            lola_event_id.h
            lola_event_instance_deployment.h
            lola_field_id.h
            lola_field_instance_deployment.h
            someip_event_instance_deployment.h
            someip_field_instance_deployment.h
            shm_size_calc_mode.h
            tracing_configuration.h
            global_configuration.h
            configuration_common_resources.h
)

# Dependencies
target_link_libraries(com_impl_configuration
    PUBLIC
        com_impl_tracing
        score::score_containers
        score::score_language
        score::score_mw
        score::score_result
        score::score_json
        score::score_futurecpp
    PRIVATE
        score::score_mw_log_core
)

# Note: This would need to include all the configuration header files
# For now, creating a placeholder - only include files that actually exist
# target_sources(com_impl_configuration
#     INTERFACE
#         FILE_SET HEADERS
#         BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
#         FILES
#             ara_com_config.h
#             ara_com_config_parser.h
#             ara_com_config_validator.h
#             configuration_exception.h
#             configuration_validation_exception.h
#             ara_com_config_schema_provider.h
# )

target_link_libraries(com_impl_configuration
    INTERFACE
        nlohmann_json::nlohmann_json
        score::score_filesystem
        score::score_mw
)

# Install the JSON schema file
install(FILES ara_com_config_schema.json
    DESTINATION share/score-communication/schema
)

# Add aliases for exported targets
add_library(score-communication::com_impl_configuration ALIAS com_impl_configuration)

# Install targets
install(TARGETS com_impl_configuration
    EXPORT score-communicationTargets
    FILE_SET HEADERS DESTINATION include/score/mw/com/impl/configuration
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Test resources library for ConfigurationStore
add_library(com_impl_configuration_test_resources
    test/configuration_store.cpp
    test/configuration_test_resources.cpp
)

target_include_directories(com_impl_configuration_test_resources
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_sources(com_impl_configuration_test_resources
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${CMAKE_SOURCE_DIR}/score/mw/com/impl/configuration
        FILES
            test/configuration_store.h
            test/configuration_test_resources.h
)

target_link_libraries(com_impl_configuration_test_resources
    PUBLIC
        com_impl_configuration
        score::score_containers
        score::score_language
)

# Add alias for test resources
add_library(score-communication::com_impl_configuration_test_resources ALIAS com_impl_configuration_test_resources)

# Install test resources
install(TARGETS com_impl_configuration_test_resources
    EXPORT score-communicationTargets
    FILE_SET HEADERS DESTINATION include/score/mw/com/impl/configuration/test
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
