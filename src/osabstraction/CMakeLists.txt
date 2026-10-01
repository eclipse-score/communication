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

file(GLOB_RECURSE OSABSTRACTION_SOURCE CONFIGURE_DEPENDS *.cpp)

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    list(REMOVE_ITEM OSABSTRACTION_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/multiplex/MultiplexerSelect.cpp)
    list(REMOVE_ITEM OSABSTRACTION_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/messagepassing/MessagePassingSender.cpp)
    list(REMOVE_ITEM OSABSTRACTION_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/messagepassing/MessagePassingServer.cpp)
else()
    list(REMOVE_ITEM OSABSTRACTION_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/multiplex/MultiplexerEpoll.cpp)
endif()

if (NOT ENABLE_TLS)
    list(REMOVE_ITEM OSABSTRACTION_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/socket/SecureConnector.cpp)
endif()

message(STATUS "* OS-Abst Project base: ${CMAKE_CURRENT_SOURCE_DIR}")
message(STATUS "* OS-Abst Sources: ${OSABSTRACTION_SOURCE}")

set(LIBRARY_NAME "osabstraction")
add_library(${LIBRARY_NAME} STATIC ${OSABSTRACTION_SOURCE})
lgsomeip_configure_target(${LIBRARY_NAME})
target_include_directories(${LIBRARY_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(${LIBRARY_NAME} ${ADDITIONAL_LIBS})

# ALIAS

add_library(lgsomeip::${LIBRARY_NAME} ALIAS ${LIBRARY_NAME})

# targets EXPORT

set_target_properties(${LIBRARY_NAME} PROPERTIES
        OUTPUT_NAME ${LIBRARY_NAME}
        POSITION_INDEPENDENT_CODE ON
)

install(TARGETS ${LIBRARY_NAME} EXPORT ${LGSOMEIP_EXPORT_NAME}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
