# LOLA binding service discovery test utilities
set(TARGET_NAME lola_service_discovery_test)

# Source files
set(SOURCES
    destructor_notifier.cpp
    file_system_guard.cpp
    service_discovery_client_test_fixtures.cpp
    service_discovery_client_test_resources.cpp
)

# Header files
set(HEADERS
    destructor_notifier.h
    file_system_guard.h
    service_discovery_client_test_fixtures.h
    service_discovery_client_test_resources.h
)

# Create test utilities library
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
        com_impl_bindings_lola_service_discovery
        lola_service_discovery_client
    PRIVATE
        score::score_mw_log_core
        GTest::gtest
        GTest::gmock
)

# Header file sets for proper installation
target_sources(${TARGET_NAME}
    PUBLIC FILE_SET HEADERS
    BASE_DIRS ${CMAKE_SOURCE_DIR}/score/mw/com/impl/bindings/lola/service_discovery/test
    FILES ${HEADERS}
)

# Install target
install(TARGETS ${TARGET_NAME}
    EXPORT score-communicationTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    FILE_SET HEADERS DESTINATION include/score/mw/com/impl/bindings/lola/service_discovery/test
)
