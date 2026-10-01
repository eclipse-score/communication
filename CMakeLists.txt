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

# 3.18 is required by the FetchContent SOURCE_SUBDIR option used below; on older
# CMake that option is silently ignored and the fetched dependency's own project
# would be added to this build.
cmake_minimum_required(VERSION 3.18)
project(lgsomeip VERSION 0.9.0)

set(LGSOMEIP_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(LGSOMEIP_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}")
set(LGSOMEIP_VERSION "${PROJECT_VERSION}")
set(LGSOMEIP_EXPORT_NAME "lgsomeip-targets")
set(LGSOMEIP_DAEMON_IMPLEMENTATION_SOURCES
    ${LGSOMEIP_SOURCE_DIR}/src/someip/packetrouter/PacketRouterHost.cpp
    ${LGSOMEIP_SOURCE_DIR}/src/someip/runtime/ServiceManager.cpp
)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# include GNU install directories such as CMAKE_INSTALL_DATADIR
include(GNUInstallDirs)
include(CTest)

# BUILD SYSTEM Configuration
message(STATUS "OS is ${CMAKE_SYSTEM_NAME}")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(OS "LINUX")
    set(CMAKE_CXX_EXTENSIONS OFF)
elseif(CMAKE_SYSTEM_NAME STREQUAL "QNX")
    set(OS "QNX")
    set(ADDITIONAL_LIBS socket)
    set(CMAKE_CXX_EXTENSIONS ON)
else()
    message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM_NAME}. LG SOME/IP supports Linux and QNX.")
endif()

# Build options and compile-time configuration
option(ENABLE_TLS "Build TLS/DTLS secure connection support using OpenSSL" OFF)
option(ENABLE_DLT "Use COVESA DLT as the logging backend instead of std::cout" OFF)
option(ENABLE_SOMEIP_TP "Enable SOME/IP Transport Protocol support" OFF)
option(ENABLE_SOMEIP_PACKET_FILTERING "Enable SOME/IP packet filtering" OFF)
option(ENABLE_SOMEIP_DELIVERY_STATISTICS "Enable SOME/IP packet delivery statistics" OFF)
option(ENABLE_TEST_BUILD "Build unit tests and coverage instrumentation" OFF)
option(ENABLE_EXAMPLES "Build example applications" ON)
option(ENABLE_TOOLS "Build diagnostic tools" ON)
option(ENABLE_SOMEIP_IPC "Enable extended SOME/IP IPC routing support" OFF)

if(CMAKE_SYSTEM_NAME STREQUAL "QNX")
    set(_enable_qnx_message_passing_default ON)
else()
    set(_enable_qnx_message_passing_default OFF)
endif()
option(ENABLE_QNX_MESSAGE_PASSING "Use QNX message passing for local IPC" ${_enable_qnx_message_passing_default})
unset(_enable_qnx_message_passing_default)

if(ENABLE_QNX_MESSAGE_PASSING AND NOT CMAKE_SYSTEM_NAME STREQUAL "QNX")
    message(FATAL_ERROR "ENABLE_QNX_MESSAGE_PASSING requires a QNX build")
endif()

# Application control entry ID for management of application connections.
set(APPLICATION_MGMT_ENTRY "0xF0" CACHE STRING "SOME/IP application management entry ID")
set(SOMEIP_UDP_MAX_PAYLOAD_SIZE "1400" CACHE STRING "Maximum SOME/IP UDP payload size in bytes")

# TLS/DTLS secure connection support (OpenSSL-backed, opt-in)
# Resolution order:
#   1. An explicit path via -DOPENSSL_ROOT_DIR=<path>.
#   2. OpenSSL present in the standard system paths.
if(ENABLE_TLS)
    find_package(OpenSSL REQUIRED)
    # Consumers find OpenSSL via find_dependency(OpenSSL) in the installed package config.
    list(APPEND ADDITIONAL_LIBS OpenSSL::SSL OpenSSL::Crypto)
endif()

# Logging backend: DLT (COVESA Diagnostic Log and Trace), opt-in.
# Resolution order:
#   1. CMake package: find_package(automotive-dlt).
#   2. pkg-config: pkg_check_modules(automotive-dlt) — creates PkgConfig::DLT.
#   If neither is found, a FATAL_ERROR is issued (install libdlt-dev).
set(LGSOMEIP_DLT_USE_CMAKE_PACKAGE OFF)
if(ENABLE_DLT)
    find_package(automotive-dlt QUIET)
    if(automotive-dlt_FOUND)
        set(LGSOMEIP_DLT_USE_CMAKE_PACKAGE ON)
        list(APPEND ADDITIONAL_LIBS Genivi::dlt)
    else()
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(DLT QUIET IMPORTED_TARGET automotive-dlt)
        if(DLT_FOUND)
            list(APPEND ADDITIONAL_LIBS PkgConfig::DLT)
        else()
            message(FATAL_ERROR
                "ENABLE_DLT is ON but automotive-dlt was not found.\n"
                "Install the DLT development package (e.g. 'apt install libdlt-dev') "
                "or set automotive-dlt_DIR to the CMake config directory.")
        endif()
    endif()
endif()

# Compile definitions shared by project targets.
set(LGSOMEIP_COMPILE_DEFINITIONS
    APPLICATION_MGMT_ENTRY=${APPLICATION_MGMT_ENTRY}
    SOMEIP_UDP_MAX_PAYLOAD_SIZE=${SOMEIP_UDP_MAX_PAYLOAD_SIZE}
)
foreach(_feature IN ITEMS
        ENABLE_TLS
        ENABLE_DLT
        ENABLE_SOMEIP_TP
        ENABLE_SOMEIP_PACKET_FILTERING
        ENABLE_SOMEIP_DELIVERY_STATISTICS
        ENABLE_QNX_MESSAGE_PASSING
        ENABLE_SOMEIP_IPC)
    if(${_feature})
        list(APPEND LGSOMEIP_COMPILE_DEFINITIONS ${_feature})
    endif()
endforeach()

function(lgsomeip_configure_target target)
    target_compile_features(${target} PRIVATE cxx_std_14)
    target_compile_definitions(${target} PRIVATE ${LGSOMEIP_COMPILE_DEFINITIONS} ${OS})
    target_compile_options(${target} PRIVATE -Wall)

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_compile_options(${target} PRIVATE -pthread)
        get_target_property(_target_type ${target} TYPE)
        if(_target_type MATCHES "EXECUTABLE|SHARED_LIBRARY|MODULE_LIBRARY")
            target_link_options(${target} PRIVATE -pthread)
        endif()
    endif()

    if(ENABLE_TEST_BUILD)
        target_compile_options(${target} PRIVATE -fprofile-arcs -ftest-coverage)
        get_target_property(_target_type ${target} TYPE)
        if(_target_type MATCHES "EXECUTABLE|SHARED_LIBRARY|MODULE_LIBRARY")
            target_link_options(${target} PRIVATE --coverage)
        endif()
    endif()
endfunction()

# Test dependencies
if(ENABLE_TEST_BUILD)
    find_package(GTest REQUIRED)
    add_library(lgsomeip-test-dependencies INTERFACE)
    target_link_libraries(lgsomeip-test-dependencies INTERFACE GTest::gtest_main)
    add_library(lgsomeip::test-dependencies ALIAS lgsomeip-test-dependencies)
endif()

if(ENABLE_TEST_BUILD)
    message(STATUS "* Coverage Test Enabled")
endif()

# RapidJSON (header-only) dependency for configuration parsing.
# Resolution order:
#   1. An explicit path via -DRAPIDJSON_INCLUDE_DIR=<path>.
#   2. RapidJSON headers already present in the standard include paths.
#   3. Otherwise, download it at configure time via FetchContent.
if(NOT RAPIDJSON_INCLUDE_DIR)
    find_path(RAPIDJSON_INCLUDE_DIR NAMES rapidjson/document.h)
endif()

if(NOT RAPIDJSON_INCLUDE_DIR)
    message(STATUS "RapidJSON not found; fetching it at configure time")
    include(FetchContent)
    FetchContent_Declare(
        rapidjson
        GIT_REPOSITORY https://github.com/Tencent/rapidjson.git
        GIT_TAG f54b0e47a08782a6131cc3d60f94d038fa6e0a51
        GIT_SHALLOW TRUE
        SOURCE_SUBDIR none # header-only: skip RapidJSON's own CMake project
    )
    FetchContent_MakeAvailable(rapidjson)
    set(RAPIDJSON_INCLUDE_DIR ${rapidjson_SOURCE_DIR}/include)
endif()

message(STATUS "Using RapidJSON headers from ${RAPIDJSON_INCLUDE_DIR}")

# Build target helpers and internal modules.
set(SOMEIP_INTERNAL_INCLUDE_DIRS
    ${LGSOMEIP_SOURCE_DIR}/src/osabstraction
    ${LGSOMEIP_SOURCE_DIR}/src/someip
    ${LGSOMEIP_SOURCE_DIR}/third_party/e2e_protection/include
    ${RAPIDJSON_INCLUDE_DIR}
)

function(someip_configure_target target)
    target_include_directories(${target} PRIVATE ${SOMEIP_INTERNAL_INCLUDE_DIRS})
    lgsomeip_configure_target(${target})
endfunction()

add_subdirectory(${LGSOMEIP_SOURCE_DIR}/src/osabstraction ${LGSOMEIP_BINARY_DIR}/src/osabstraction)
add_subdirectory(${LGSOMEIP_SOURCE_DIR}/third_party/e2e_protection ${LGSOMEIP_BINARY_DIR}/third_party/e2e_protection)
add_subdirectory(${LGSOMEIP_SOURCE_DIR}/src/someip ${LGSOMEIP_BINARY_DIR}/src/someip)
add_subdirectory(${LGSOMEIP_SOURCE_DIR}/daemon ${LGSOMEIP_BINARY_DIR}/daemon)
add_subdirectory(${LGSOMEIP_SOURCE_DIR}/third_party/vsomeip_interface ${LGSOMEIP_BINARY_DIR}/third_party/vsomeip_interface)

if(ENABLE_TOOLS)
    add_subdirectory(${LGSOMEIP_SOURCE_DIR}/tools/dump-someipservices ${LGSOMEIP_BINARY_DIR}/tools/dump-someipservices)
endif()

if(ENABLE_EXAMPLES AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_subdirectory(${LGSOMEIP_SOURCE_DIR}/examples/someip/simple-event ${LGSOMEIP_BINARY_DIR}/examples/someip/simple-event)
    add_subdirectory(${LGSOMEIP_SOURCE_DIR}/examples/someip/simple-method ${LGSOMEIP_BINARY_DIR}/examples/someip/simple-method)
endif()

if(ENABLE_TEST_BUILD AND BUILD_TESTING)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_subdirectory(${LGSOMEIP_SOURCE_DIR}/tests/unit/osabstraction ${LGSOMEIP_BINARY_DIR}/tests/unit/osabstraction)
        add_subdirectory(${LGSOMEIP_SOURCE_DIR}/tests/unit/someip ${LGSOMEIP_BINARY_DIR}/tests/unit/someip)
        add_subdirectory(${LGSOMEIP_SOURCE_DIR}/tests/component ${LGSOMEIP_BINARY_DIR}/tests/component)
        add_subdirectory(${LGSOMEIP_SOURCE_DIR}/tests/integration ${LGSOMEIP_BINARY_DIR}/tests/integration)
        add_subdirectory(${LGSOMEIP_SOURCE_DIR}/tests/latency ${LGSOMEIP_BINARY_DIR}/tests/latency)

        add_dependencies(unittest_osabstraction
            unittest_osabstraction_component
            unittest_osabstraction_integration
        )
        add_dependencies(unittest_someip
            unittest_someip_component
            unittest_someip_integration
        )
    endif()
    add_subdirectory(${LGSOMEIP_SOURCE_DIR}/tests/unit/serialization ${LGSOMEIP_BINARY_DIR}/tests/unit/serialization)
endif()

# Package configuration

include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${PROJECT_SOURCE_DIR}/cmake/vsomeip-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/vsomeip-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/vsomeip
    NO_SET_AND_CHECK_MACRO
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/vsomeip-config-version.cmake
    COMPATIBILITY ExactVersion
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/vsomeip-config.cmake
          ${CMAKE_CURRENT_BINARY_DIR}/vsomeip-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/vsomeip
)

install(EXPORT ${LGSOMEIP_EXPORT_NAME}
    FILE vsomeip-implementation-targets.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/vsomeip
        NAMESPACE lgsomeip::
)
