# Mock binding implementation for COM
set(TARGET_NAME com_impl_bindings_mock)

# Source files
set(SOURCES
    generic_proxy_event.cpp
    proxy.cpp
    proxy_event.cpp
    sample_ptr.cpp
    skeleton.cpp
    skeleton_event.cpp
)

# Header files
set(HEADERS
    generic_proxy_event.h
    proxy.h
    proxy_event.h
    sample_ptr.h
    skeleton.h
    skeleton_event.h
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
        message_passing
    PRIVATE
        score::score_mw_log_core
        GTest::gmock
)

# Header file sets for proper installation
target_sources(${TARGET_NAME}
    PUBLIC FILE_SET HEADERS
    BASE_DIRS ${CMAKE_SOURCE_DIR}/score/mw/com/impl/bindings/mock_binding
    FILES ${HEADERS}
)

# Add subdirectories
add_subdirectory(tracing)

# Install target
install(TARGETS ${TARGET_NAME}
    EXPORT score-communicationTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    FILE_SET HEADERS DESTINATION include/score/mw/com/impl/bindings/mock_binding
)
