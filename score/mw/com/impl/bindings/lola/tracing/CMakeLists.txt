# LOLA binding tracing implementation
set(TARGET_NAME com_impl_bindings_lola_tracing)

# Source files
set(SOURCES
    tracing_runtime.cpp
)

# Header files
set(HEADERS
    tracing_runtime.h
)

# Test files
set(TEST_SOURCES
    tracing_runtime_test.cpp
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
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_SOURCE_DIR}
)

# Dependencies
target_link_libraries(${TARGET_NAME}
    PUBLIC
    PRIVATE
        score::score_analysis_tracing_common_interface_types
        score::score_mw_log_core
)

# Header file sets for proper installation
target_sources(${TARGET_NAME}
    PUBLIC FILE_SET HEADERS
    BASE_DIRS ${CMAKE_SOURCE_DIR}/score/mw/com/impl/bindings/lola/tracing
    FILES ${HEADERS}
)

# Install target
install(TARGETS ${TARGET_NAME}
    EXPORT score-communicationTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    FILE_SET HEADERS DESTINATION include/score/mw/com/impl/bindings/lola/tracing
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
                lola_test_resources  # Add the skeleton test resources library
                -Wl,--whole-archive
                com_impl_bindings_lola_service_discovery
                lola_service_discovery_client
                -Wl,--no-whole-archive
                com_impl_bindings_lola  # Link to main LOLA library which includes all dependencies
                score::score_filesystem_factory_fake
                score::score_filesystem_filestream_fake
                GTest::gtest
                GTest::gtest_main
                GTest::gmock
                com_impl_configuration
                score::score_analysis_tracing_interface_types
                score::score_analysis_tracing_mock
                score::score_mw_log_core
                score::score_mw_log_detail
                score::score_os
                score::score_futurecpp
                -Wl,--whole-archive
                score::score_memory_split_string_view
                -Wl,--no-whole-archive
        )
        
        target_include_directories(${test_target}
            PRIVATE
                ${CMAKE_SOURCE_DIR}
                ${CMAKE_CURRENT_SOURCE_DIR}
        )
        
        add_test(NAME ${test_target} COMMAND ${test_target})
    endforeach()
endif()
