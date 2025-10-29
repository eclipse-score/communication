# LOLA binding service discovery client implementation
set(TARGET_NAME lola_service_discovery_client)

# Source files
set(SOURCES
    service_discovery_client.cpp
)

# Header files
set(HEADERS
    service_discovery_client.h
)

# Test files
set(TEST_SOURCES
    service_discovery_client_find_service_test.cpp
    service_discovery_client_offer_service_test.cpp
    service_discovery_client_sequence_test.cpp
    service_discovery_client_start_find_service_test.cpp
    service_discovery_client_stop_find_service_test.cpp
    service_discovery_client_stop_offer_service_test.cpp
    service_discovery_client_worker_thread_test.cpp
)

# Create implementation library
add_library(${TARGET_NAME} STATIC ${SOURCES})

# Set target properties
set_target_properties(${TARGET_NAME} PROPERTIES
    OUTPUT_NAME "${TARGET_NAME}"
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Include directories
target_include_directories(${TARGET_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}
)

# Dependencies
target_link_libraries(${TARGET_NAME}
    PUBLIC
        score::score_filesystem
        score::score_filesystem_i_standard
        score::score_os
        score::score_result
        score::score_safecpp
        score::score_futurecpp
        score::score_concurrency_future
        score::score_memory
        score::score_memory_split_string_view
        score::score_mw_log_configuration
        score::score_mw_log_detail
        score::score_mw_log_core
        com_impl_core
        com_impl_bindings_lola_service_discovery
    PRIVATE
        score::score_executor
        score::score_mw_log_core
        score::score_mw_log_configuration
)

# Header file sets for proper installation
target_sources(${TARGET_NAME}
    PUBLIC FILE_SET HEADERS
    BASE_DIRS ${CMAKE_SOURCE_DIR}/score/mw/com/impl/bindings/lola/service_discovery/client
    FILES ${HEADERS}
)

# Install target
install(TARGETS ${TARGET_NAME}
    EXPORT score-communicationTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    FILE_SET HEADERS DESTINATION include/score/mw/com/impl/bindings/lola/service_discovery/client
)

# Add tests if requested
if(SCORE_BUILD_TESTING)
    foreach(test_file ${TEST_SOURCES})
        get_filename_component(test_name ${test_file} NAME_WE)
        set(test_target "${TARGET_NAME}_${test_name}")
        
        add_executable(${test_target} ${test_file})
        target_link_libraries(${test_target}
            PRIVATE
                ${TARGET_NAME}
                com_impl_configuration
                lola_service_discovery_test
                score::score_filesystem
                score::score_filesystem_factory_fake
                score::score_filesystem_filestream_fake
                score::score_mw_log_configuration
                score::score_os
                score::score_os_fake
                score::score_memory_split_string_view
                GTest::gtest
                GTest::gtest_main
                GTest::gmock
        )
        
        target_include_directories(${test_target}
            PRIVATE
                ${CMAKE_SOURCE_DIR}
                ${CMAKE_CURRENT_SOURCE_DIR}
        )
        
        add_test(NAME ${test_target} COMMAND ${test_target})
    endforeach()
endif()
