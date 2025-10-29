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

# Message passing library
add_library(com_message_passing
    # Core message passing implementation
    message.cpp
    non_blocking_sender.cpp
    receiver_factory.cpp
    sender_factory.cpp
    serializer.cpp
    shared_properties.cpp
    
    # Message queue implementations
    mqueue/mqueue_receiver_factory.cpp
    mqueue/mqueue_receiver_traits.cpp
    mqueue/mqueue_sender_factory.cpp
    mqueue/mqueue_sender_traits.cpp
)

# Add QNX-specific files only if on QNX platform
if(CMAKE_SYSTEM_NAME STREQUAL "QNX")
    target_sources(com_message_passing PRIVATE
        qnx/resmgr_receiver_factory.cpp
        qnx/resmgr_receiver_traits.cpp
        qnx/resmgr_sender_factory.cpp
        qnx/resmgr_sender_traits.cpp
        qnx/resmgr_traits_common.cpp
    )
endif()

target_include_directories(com_message_passing
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_sources(com_message_passing
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
        FILES
            # Main header files
            i_receiver.h
            i_sender.h
            message.h
            non_blocking_sender.h
            receiver.h
            receiver_config.h
            receiver_factory.h
            receiver_factory_impl.h
            receiver_mock.h
            receiver_traits_mock.h
            sender.h
            sender_config.h
            sender_factory.h
            sender_factory_impl.h
            sender_mock.h
            sender_traits_mock.h
            serializer.h
            shared_properties.h
            # Message queue headers
            mqueue/mqueue_receiver_traits.h
            mqueue/mqueue_sender_traits.h
)

# Add QNX-specific headers only if on QNX platform
if(CMAKE_SYSTEM_NAME STREQUAL "QNX")
    target_sources(com_message_passing
        PUBLIC
            FILE_SET HEADERS
            FILES
                qnx/resmgr_receiver_traits.h
                qnx/resmgr_sender_traits.h
                qnx/resmgr_traits_common.h
    )
endif()

# Link with required dependencies
    target_link_libraries(com_message_passing PUBLIC
        score::score_concurrency_future
        score::score_filesystem
        score::score_filesystem_i_standard
        score::score_memory_split_string_view
        score::score_mw_log_configuration
        score::score_mw_log_detail
        score::score_mw_log_core
        score::score_os
        score::score_result
    )

# Separate install for message_passing headers to ensure correct subdirectory
install(TARGETS com_message_passing
    EXPORT score-communicationTargets
    FILE_SET HEADERS DESTINATION include/score/mw/com/message_passing
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)