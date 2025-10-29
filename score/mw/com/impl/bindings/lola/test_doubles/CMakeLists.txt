# LOLA binding test doubles implementation
set(TARGET_NAME lola_test_doubles)

# Source files
set(SOURCES
    fake_memory_resource.cpp
    fake_mocked_service_data.cpp
    fake_service_data.cpp
)

# Header files
set(HEADERS
    fake_memory_resource.h
    fake_mocked_service_data.h
    fake_service_data.h
)

# Create test doubles library
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
        score::score_mw_log_core
        GTest::gmock
)

# Header file sets for proper installation
target_sources(${TARGET_NAME}
    PUBLIC FILE_SET HEADERS
    BASE_DIRS ${CMAKE_SOURCE_DIR}/score/mw/com/impl/bindings/lola/test_doubles
    FILES ${HEADERS}
)

# Install target
install(TARGETS ${TARGET_NAME}
    EXPORT score-communicationTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    FILE_SET HEADERS DESTINATION include/score/mw/com/impl/bindings/lola/test_doubles
)
