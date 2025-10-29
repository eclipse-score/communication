# LOLA binding test resources and components
set(TARGET_NAME lola_test_resources)

# Source files
set(SOURCES
    proxy_event_test_resources.cpp
    service_discovery_test_resources.cpp
    skeleton_event_test_resources.cpp
    skeleton_test_resources.cpp
    transaction_log_test_resources.cpp
)

# Header files
set(HEADERS
    proxy_event_test_resources.h
    service_discovery_test_resources.h
    skeleton_event_test_resources.h
    skeleton_test_resources.h
    transaction_log_test_resources.h
)

# Test files
set(TEST_SOURCES
    proxy_component_test.cpp
    skeleton_component_test.cpp
    skeleton_event_component_test.cpp
)

# Create test resources library
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
        com_impl_bindings_lola
    PRIVATE
        score::score_analysis_tracing_common_interface_types
        score::score_mw_log_core
)

# Header file sets for proper installation
target_sources(${TARGET_NAME}
    PUBLIC FILE_SET HEADERS
    BASE_DIRS ${CMAKE_SOURCE_DIR}/score/mw/com/impl/bindings/lola/test
    FILES ${HEADERS}
)

# Install target
install(TARGETS ${TARGET_NAME}
    EXPORT score-communicationTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    FILE_SET HEADERS DESTINATION include/score/mw/com/impl/bindings/lola/test
)

# Add tests if requested
if(SCORE_BUILD_TESTING)
    foreach(test_file ${TEST_SOURCES})
        get_filename_component(test_name ${test_file} NAME_WE)
        set(test_target "lola_${test_name}")
        
        add_executable(${test_target} ${test_file})
        target_link_libraries(${test_target}
            PRIVATE
                ${TARGET_NAME}
                lola_test_doubles
                -Wl,--whole-archive
                com_impl_bindings_lola_service_discovery
                lola_service_discovery_client
                score::score_memory_split_string_view
                -Wl,--no-whole-archive
                com_impl_bindings_lola
                score::score_filesystem_factory_fake
                score::score_filesystem_filestream_fake
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
