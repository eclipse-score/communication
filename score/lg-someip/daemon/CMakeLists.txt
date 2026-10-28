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

file(GLOB_RECURSE SOMEIP_DAEMON_SOURCE CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/someipd.cpp
)

message(STATUS "* SOMEIP DAEMON Project base: ${CMAKE_CURRENT_SOURCE_DIR}")
message(STATUS "* SOMEIP DAEMON Sources: ${SOMEIP_DAEMON_SOURCE}")

set(DAEMON_NAME "someip-daemon")

add_library(lgsomeip-daemon-core STATIC
    ${LGSOMEIP_DAEMON_IMPLEMENTATION_SOURCES}
)
someip_configure_target(lgsomeip-daemon-core)
target_compile_definitions(lgsomeip-daemon-core PRIVATE SOMEIP_VERSION="${LGSOMEIP_VERSION}")
target_link_libraries(lgsomeip-daemon-core
    PUBLIC
        lgsomeip::lgsomeip-shared
)

add_library(lgsomeip::lgsomeip-daemon-core ALIAS lgsomeip-daemon-core)

add_executable(${DAEMON_NAME} ${SOMEIP_DAEMON_SOURCE})
someip_configure_target(${DAEMON_NAME})
target_compile_definitions(${DAEMON_NAME} PRIVATE SOMEIP_VERSION="${LGSOMEIP_VERSION}")

target_link_libraries(${DAEMON_NAME}
    PRIVATE
        lgsomeip::lgsomeip-daemon-core)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    file(RELATIVE_PATH DAEMON_INSTALL_TO_LIB_DIR
        "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}"
        "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}"
    )
    set_target_properties(${DAEMON_NAME} PROPERTIES
        INSTALL_RPATH "\$ORIGIN/${DAEMON_INSTALL_TO_LIB_DIR}"
    )
endif()

install(TARGETS ${DAEMON_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})
