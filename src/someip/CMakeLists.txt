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


message(STATUS "* CMAKE_INSTALL_LIBDIR: ${CMAKE_INSTALL_LIBDIR}")

file(GLOB_RECURSE SOMEIP_SOURCE_LIB CONFIGURE_DEPENDS
    endpoint/*.cpp
    message/*.cpp
    packetrouter/PacketRouterProxy.cpp
    runtime/ApplicationManager.cpp
    runtime/EventManager.cpp
    utils/byteorder/*.cpp
    utils/time/*.cpp
    utils/thread/*.cpp
    config/*.cpp
)

if(NOT ENABLE_QNX_MESSAGE_PASSING)
    list(REMOVE_ITEM SOMEIP_SOURCE_LIB ${CMAKE_CURRENT_SOURCE_DIR}/endpoint/EndpointMessagePassing.cpp)
endif()

message(STATUS "* SOMEIP Project base: ${CMAKE_CURRENT_SOURCE_DIR}")
message(STATUS "* SOMEIP Sources: ${SOMEIP_SOURCE_LIB}")

add_library(lgsomeip-common-objects OBJECT ${SOMEIP_SOURCE_LIB})
someip_configure_target(lgsomeip-common-objects)
target_compile_definitions(lgsomeip-common-objects PRIVATE SOMEIP_VERSION="${LGSOMEIP_VERSION}")
set_target_properties(lgsomeip-common-objects PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Reuse the common object files for the shared library as well.
set(LIBRARY_NAME "lgsomeip-shared")
add_library(${LIBRARY_NAME} SHARED $<TARGET_OBJECTS:lgsomeip-common-objects>)
someip_configure_target(${LIBRARY_NAME})

target_link_libraries(${LIBRARY_NAME}
    PUBLIC
        lgsomeip::osabstraction
        lgsomeip::vsomeip-e2e
        ${ADDITIONAL_LIBS}
)

add_library(lgsomeip::${LIBRARY_NAME} ALIAS ${LIBRARY_NAME})

set_target_properties(${LIBRARY_NAME} PROPERTIES
        OUTPUT_NAME "lgsomeip"
        POSITION_INDEPENDENT_CODE ON
)

install(TARGETS ${LIBRARY_NAME} EXPORT ${LGSOMEIP_EXPORT_NAME}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
