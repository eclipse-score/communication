# *******************************************************************************
# Copyright (c) 2024-2025 Contributors to the Eclipse Foundation
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

cmake_minimum_required(VERSION 3.20)

# Prevent in-source builds to keep the source tree clean
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR
        "In-source builds are not allowed. Please create a separate build directory:\n"
        "  mkdir build\n"
        "  cd build\n"
        "  cmake ..\n"
        "Or use: cmake -B build -S .")
endif()

project(score_communication
    VERSION 1.0.0
    DESCRIPTION "SCORE Communication Library"
    LANGUAGES CXX
)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler warnings and features
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(
        -Wno-maybe-uninitialized
        -Wno-error
    )
endif()

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Enable testing to access fake libraries from baselibs
option(SCORE_BUILD_TESTING "Build test programs" OFF)
if(SCORE_BUILD_TESTING)
    include(CTest)
endif()

# Platform detection
if(CMAKE_SYSTEM_NAME STREQUAL "QNX")
    set(TARGET_QNX TRUE)
    add_compile_definitions(TARGET_QNX=1)
else()
    set(TARGET_QNX FALSE)
endif()

# Find required dependencies
set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../install" ${CMAKE_PREFIX_PATH})
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Find score-baselibs
find_package(score-baselibs REQUIRED)

# Add global include directories for score headers
include_directories(${CMAKE_PREFIX_PATH}/include)

# Find other dependencies
find_package(GTest REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(Boost REQUIRED COMPONENTS program_options)

# Add critical missing baselibs libraries for LOLA binding
if(TARGET score::memory_shared_sync)
    set(SCORE_MEMORY_SHARED_SYNC_LIB score::memory_shared_sync)
else()
    find_library(SCORE_MEMORY_SHARED_SYNC_LIB score_memory_shared_sync PATHS ${CMAKE_PREFIX_PATH}/lib REQUIRED)
endif()

if(TARGET score::os_acl)
    set(SCORE_OS_ACL_LIB score::os_acl)
else()
    find_library(SCORE_OS_ACL_LIB score_os_acl PATHS ${CMAKE_PREFIX_PATH}/lib)
endif()

if(TARGET score::memory_lazy_split)
    set(SCORE_MEMORY_LAZY_SPLIT_LIB score::memory_lazy_split)
else()
    find_library(SCORE_MEMORY_LAZY_SPLIT_LIB score_memory_lazy_split PATHS ${CMAKE_PREFIX_PATH}/lib)
endif()

if(TARGET score::os_inotify)
    set(SCORE_OS_INOTIFY_LIB score::os_inotify)
else()
    find_library(SCORE_OS_INOTIFY_LIB score_os_inotify PATHS ${CMAKE_PREFIX_PATH}/lib)
endif()

if(TARGET score::mw_log_interface)
    set(SCORE_MW_LOG_INTERFACE_LIB score::mw_log_interface)
else()
    find_library(SCORE_MW_LOG_INTERFACE_LIB score_mw_log_interface PATHS ${CMAKE_PREFIX_PATH}/lib)
endif()

# Optional: Find json schema validator for configuration validation
include(FindJsonSchemaValidator)

# Testing support using the SCORE_BUILD_TESTING flag
if(SCORE_BUILD_TESTING)
    enable_testing()
    include(GoogleTest)
endif()

# Add subdirectories
add_subdirectory(score/message_passing)
add_subdirectory(score/mw)

# Install configuration
include(GNUInstallDirs)

# Create and install package config files
include(CMakePackageConfigHelpers)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/score-communicationConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/score-communicationConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/score-communication
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/score-communicationConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/score-communicationConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/score-communicationConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/score-communication
)

# Install the export targets
install(EXPORT score-communicationTargets
    FILE score-communicationTargets.cmake
    NAMESPACE score-communication::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/score-communication
)
