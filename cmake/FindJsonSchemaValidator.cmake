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

include(FetchContent)

# Fetch json-schema-validator if not found
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(JSON_SCHEMA_VALIDATOR QUIET nlohmann_json_schema_validator)
endif()

if(NOT JSON_SCHEMA_VALIDATOR_FOUND)
    message(STATUS "Fetching json-schema-validator from GitHub")
    
    FetchContent_Declare(
        json_schema_validator
        GIT_REPOSITORY https://github.com/pboettch/json-schema-validator.git
        GIT_TAG 2.1.0
        GIT_SHALLOW TRUE
    )
    
    FetchContent_MakeAvailable(json_schema_validator)
    
    # Disable warnings as errors for this external dependency
    if(TARGET nlohmann_json_schema_validator)
        target_compile_options(nlohmann_json_schema_validator PRIVATE
            $<$<CXX_COMPILER_ID:GNU,Clang>:-Wno-error>
        )
        add_library(json_schema_validator::json_schema_validator ALIAS nlohmann_json_schema_validator)
    endif()
else()
    # Create imported target
    add_library(json_schema_validator::json_schema_validator INTERFACE IMPORTED)
    target_link_libraries(json_schema_validator::json_schema_validator INTERFACE ${JSON_SCHEMA_VALIDATOR_LIBRARIES})
    target_include_directories(json_schema_validator::json_schema_validator INTERFACE ${JSON_SCHEMA_VALIDATOR_INCLUDE_DIRS})
endif()