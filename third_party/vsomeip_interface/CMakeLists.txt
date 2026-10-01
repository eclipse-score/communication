# Copyright (C) 2017-2026 LG Electronics Inc.
# SPDX-License-Identifier: MPL-2.0
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

file(GLOB_RECURSE SOMEIP_INTERFACE_SOURCE CONFIGURE_DEPENDS
    source/*.cpp
)

message(STATUS "* SOMEIP Interface Project base: ${CMAKE_CURRENT_SOURCE_DIR}")
message(STATUS "* SOMEIP Interface Sources: ${SOMEIP_INTERFACE_SOURCE}")

set(LIBRARY_NAME "vsomeip-if")
add_library(${LIBRARY_NAME} SHARED ${SOMEIP_INTERFACE_SOURCE})
lgsomeip_configure_target(${LIBRARY_NAME})
target_include_directories(${LIBRARY_NAME}
  PRIVATE
    ${LGSOMEIP_SOURCE_DIR}/src/osabstraction
    ${LGSOMEIP_SOURCE_DIR}/src/someip
    ${CMAKE_CURRENT_SOURCE_DIR}/../e2e_protection/include
    ${RAPIDJSON_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/source
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(${LIBRARY_NAME} PRIVATE lgsomeip-shared)

install(
  # Only the vsomeip compatibility headers are public. Core, OS abstraction,
  # serializer, and E2E headers remain private implementation details.
    DIRECTORY include/vsomeip
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    COMPONENT dev
)
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

# example, test

if(ENABLE_EXAMPLES AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_subdirectory(examples)
endif()
