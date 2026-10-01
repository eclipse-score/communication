# Copyright (C) 2017-2026 LG Electronics Inc.
# SPDX-License-Identifier: MPL-2.0
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

message(STATUS "* CMAKE_INSTALL_LIBDIR: ${CMAKE_INSTALL_LIBDIR}")

file(GLOB_RECURSE SOMEIP_E2E_SOURCE CONFIGURE_DEPENDS
    src/*.cpp
)

set(LIBRARY_NAME "vsomeip-e2e")
add_library(${LIBRARY_NAME} STATIC ${SOMEIP_E2E_SOURCE})
lgsomeip_configure_target(${LIBRARY_NAME})
target_include_directories(${LIBRARY_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/include
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
