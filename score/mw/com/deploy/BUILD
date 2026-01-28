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

load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")
load(":extract_headers_for_deps.bzl", "extract_headers")
load(":copy_headers.bzl", "copy_headers")

# ============================================================================
# Platform Configuration Settings
# ============================================================================

# Config setting for QNX7
config_setting(
    name = "qnx7",
    constraint_values = [
        "@platforms//os:qnx",
        "@score_bazel_platforms//:qnx7_1",
    ],
)

# Config setting for QNX8
config_setting(
    name = "qnx8",
    constraint_values = [
        "@platforms//os:qnx",
        "@score_bazel_platforms//:qnx8_0",
    ],
)

# Config setting for Linux aarch64 (HGY platform)
config_setting(
    name = "linux_aarch64",
    constraint_values = [
        "@platforms//os:linux",
        "@platforms//cpu:aarch64",
    ],
)

# Config setting for Linux x86_64 (standard GCC)
config_setting(
    name = "linux_x86_64",
    constraint_values = [
        "@platforms//os:linux",
        "@platforms//cpu:x86_64",
    ],
)

# ============================================================================
# Symbol Version Script
# ============================================================================

# Generate version script for symbol versioning on Linux
genrule(
    name = "generate_version_script",
    outs = ["version_script.lds"],
    cmd = """
        cat > $@ << 'EOF'
# Symbol Version Script for libscore_communication.so
# This controls which symbols are exported from the shared library

SCORE_COM_1.0 {
    global:
        # Export all public API symbols
        # C++ symbols will be mangled, so we use wildcards
        # Entire Score
        *score*; 
    local:
        # Hide all other symbols
        *;
};
EOF
    """,
    visibility = ["//visibility:public"],
)

# ============================================================================
# Dynamic Library Configuration ( Not Working - Due to Symbols Missing Issue )
# ============================================================================

# Static library that includes top level API layer as deps & uses alwayslink=True to force include all symbols.
# With this the observation com_deps has the symbols exported from direct deps (sources only) and all the transitive deps symbols are ignore ( if they are not invoked at top layer )
# Note: Had to do this for all transitive deps into this com_all_deps package due to Symbol Inclusion issue.
cc_library(
    name = "com_deps",
    features = COMPILER_WARNING_FEATURES,
    tags = ["FUSA"],
    visibility = ["//visibility:public"],
    alwayslink = True,  # Force all symbols to be included
    deps = [
        # Public API layer
        "//score/mw/com:com_error_domain",
        "//score/mw/com:runtime",
        "//score/mw/com:runtime_configuration",
        "//score/mw/com:types",
    ],
)

# Shared library for deployment containing all COM functionality
# This bundles all communication middleware code into a single .so
# All transitive dependencies are statically linked into this shared library
cc_shared_library(
    name = "score_com",
    features = COMPILER_WARNING_FEATURES,
    tags = ["FUSA"],
    visibility = ["//visibility:public"],
    deps = [
        ":com_deps",  
    ],
    # Specify which dependencies should be linked dynamically
    # Empty list means ALL dependencies are statically linked into the shared library
    dynamic_deps = [],
    # Additional linker options for dynamic library
    user_link_flags = select({
        ":linux_aarch64": [
            "-Wl,-soname,libscore_com.so",
            "-Wl,-rpath,'$$ORIGIN'",  # Set RPATH for runtime library search
            "-Wl,--version-script=$(location :generate_version_script)",  # Symbol versioning
            "-Wl,--allow-multiple-definition",  # Allow duplicate symbols (choose first)
            "-lacl",  # Link against ACL library for file permissions (needed for aarch64 - Not pulling from Deps for some reason)
            "-static-libgcc",  # Statically link libgcc
            "-static-libstdc++",  # Statically link libstdc++
        ],
        ":linux_x86_64": [
            "-Wl,-soname,libscore_com.so",
            "-Wl,-rpath,'$$ORIGIN'",  # Set RPATH for runtime library search
            "-Wl,--version-script=$(location :generate_version_script)",  # Symbol versioning
            "-Wl,--allow-multiple-definition",  # Allow duplicate symbols (choose first)
            "-static-libgcc",  # Statically link libgcc
            "-static-libstdc++",  # Statically link libstdc++
        ],
        "@platforms//os:macos": [
            "-Wl,-install_name,@rpath/libscore_com.dylib",
            "-Wl,-rpath,@loader_path",
            "-static-libgcc",
            "-static-libstdc++",
        ],
        ":qnx7": [
            "-Wl,-soname,libscore_com.so",
            "-Wl,-rpath,'$$ORIGIN'",
            "-Wl,--version-script=$(location :generate_version_script)",  # QNX supports GNU ld version scripts
            "-lslog2",  # QNX logging library (system library, must be dynamic - Not pulling from Deps for some reason)
            "-Wl,--allow-multiple-definition",  # Allow duplicate symbols (choose first)
            "-static-libgcc",
            "-static-libstdc++",
        ],
        ":qnx8": [
            "-Wl,-soname,libscore_com.so",
            "-Wl,-rpath,'$$ORIGIN'",
            "-Wl,--version-script=$(location :generate_version_script)",  # QNX supports GNU ld version scripts
            "-lslog2",  # QNX logging library (system library, must be dynamic - Not pulling from Deps for some reason)
            "-lfsnotify",  # QNX8-specific: fsnotify library (system library, must be dynamic - Not pulling from Deps for some reason)
            "-Wl,--allow-multiple-definition",  # Allow duplicate symbols (choose first)
            "-static-libgcc",
            "-static-libstdc++",
        ],
        "//conditions:default": [],
    }),
    additional_linker_inputs = select({
        ":linux_aarch64": [":generate_version_script"],
        ":linux_x86_64": [":generate_version_script"],
        ":qnx7": [":generate_version_script"],
        ":qnx8": [":generate_version_script"],
        "//conditions:default": [],
    }),
)

# ============================================================================
# Dynamic Library Configuration ( Working )
# ============================================================================

# Comprehensive aggregation library that includes ALL communication middleware components with Symbols that are needed to be included in SO.
# This ensures all transitive dependencies and their respective Public hdr symbols are properly included in the shared library
# Note: Had to do this for all transitive deps into this com_all_deps package due to Symbol Inclusion issue.
#       Without this addition. Bazel is not adding Transitive Dependency Symbols into so. Refer to SYMBOL_VERSIONING_GUIDE.md for more details on this.
cc_library(
    name = "com_all_deps",
    features = COMPILER_WARNING_FEATURES,
    tags = ["FUSA"],
    visibility = ["//visibility:public"],
    alwayslink = True,  # Force all symbols to be included
    deps = [
        # Public API layer
        "//score/mw/com:com_error_domain",
        "//score/mw/com:runtime",
        "//score/mw/com:runtime_configuration",
        "//score/mw/com:types",
        
        # Below project
        # Implementation layer - core components
        "//score/mw/com/impl:impl",
        "//score/mw/com/impl:runtime",
        "//score/mw/com/impl:error",
        "//score/mw/com/impl:instance_identifier",
        "//score/mw/com/impl:instance_specifier",
        "//score/mw/com/impl:handle_type",
        "//score/mw/com/impl:enriched_instance_identifier",
        
        # Proxy components
        "//score/mw/com/impl:proxy_base",
        "//score/mw/com/impl:proxy_event",
        "//score/mw/com/impl:proxy_event_base",
        "//score/mw/com/impl:proxy_field",
        "//score/mw/com/impl:proxy_field_base",
        "//score/mw/com/impl:generic_proxy",
        "//score/mw/com/impl:generic_proxy_event",
        "//score/mw/com/impl:proxy_binding",
        "//score/mw/com/impl:proxy_event_binding",
        "//score/mw/com/impl:generic_proxy_event_binding",
        
        # Skeleton components
        "//score/mw/com/impl:skeleton_base",
        "//score/mw/com/impl:skeleton_event",
        "//score/mw/com/impl:skeleton_event_base",
        "//score/mw/com/impl:skeleton_field",
        "//score/mw/com/impl:skeleton_field_base",
        "//score/mw/com/impl:skeleton_binding",
        "//score/mw/com/impl:skeleton_event_binding",
        
        # Service discovery
        #"//score/mw/com/impl:find_service_handle",
        #"//score/mw/com/impl:find_service_handler",
        #"//score/mw/com/impl:i_service_discovery",
        #"//score/mw/com/impl:i_service_discovery_client",
        #"//score/mw/com/impl:service_discovery",
        
        # Runtime interfaces
        #"//score/mw/com/impl:runtime_interfaces",
        #"//score/mw/com/impl:event_receive_handler",
        #"//score/mw/com/impl:scoped_event_receive_handler",
        #"//score/mw/com/impl:subscription_state",
        #"//score/mw/com/impl:binding_type",
        #"//score/mw/com/impl:service_element_type",
        #"//score/mw/com/impl:service_element_map",
        #"//score/mw/com/impl:sample_reference_tracker",
        #"//score/mw/com/impl:flag_owner",
        #"//score/mw/com/impl:traits",
        
        # Configuration
        #"//score/mw/com/impl/configuration",
        
        # Plumbing layer
        "//score/mw/com/impl/plumbing:plumbing",
        "//score/mw/com/impl/plumbing:runtime",
        #"//score/mw/com/impl/plumbing:event",
        #"//score/mw/com/impl/plumbing:field",
        #"//score/mw/com/impl/plumbing:sample_ptr",
        #"//score/mw/com/impl/plumbing:sample_allocatee_ptr",
        #"//score/mw/com/impl/plumbing:proxy_binding_factory",
        #"//score/mw/com/impl/plumbing:proxy_event_binding_factory",
        #"//score/mw/com/impl/plumbing:proxy_field_binding_factory",
        #"//score/mw/com/impl/plumbing:skeleton_binding_factory",
        #"//score/mw/com/impl/plumbing:skeleton_event_binding_factory",
        #"//score/mw/com/impl/plumbing:skeleton_field_binding_factory",
        
        # Methods
        #"//score/mw/com/impl/methods:proxy_method",
        #"//score/mw/com/impl/methods:proxy_method_base",
        #"//score/mw/com/impl/methods:proxy_method_binding",
        #"//score/mw/com/impl/methods:method_signature_element_ptr",
        
        # Tracing
        "//score/mw/com/impl/tracing:tracing_runtime",
        #"//score/mw/com/impl/tracing:i_tracing_runtime",
        #"//score/mw/com/impl/tracing:i_tracing_runtime_binding",
        #"//score/mw/com/impl/tracing:skeleton_tracing",
        "//score/mw/com/impl/tracing:skeleton_event_tracing",
        "//score/mw/com/impl/tracing:proxy_event_tracing",
        #"//score/mw/com/impl/tracing:common_event_tracing",
        #"//score/mw/com/impl/tracing:type_erased_sample_ptr",
        #"//score/mw/com/impl/tracing:skeleton_event_tracing_data",
        #"//score/mw/com/impl/tracing:proxy_event_tracing_data",
        #"//score/mw/com/impl/tracing:service_element_tracing_data",
        #"//score/mw/com/impl/tracing:trace_error",
        
        # Bindings - LOLA (main binding implementation)
        "//score/mw/com/impl/bindings/lola:lola",
        "//score/mw/com/impl/bindings/lola:proxy",
        "//score/mw/com/impl/bindings/lola:skeleton",
        "//score/mw/com/impl/bindings/lola:type_erased_sample_ptrs_guard",

        #Baselibs - specific Symbols
        "@score_baselibs//score/language/futurecpp:futurecpp",
        "@score_logging//score/mw/log",
    ],
)

# Shared library for deployment containing all COM functionality
# This bundles all communication middleware code into a single .so
# All transitive dependencies are statically linked into this shared library
cc_shared_library(
    name = "score_communication",
    features = COMPILER_WARNING_FEATURES,
    tags = ["FUSA"],
    visibility = ["//visibility:public"],
    deps = [
        ":com_all_deps",  
    ],
    # Specify which dependencies should be linked dynamically
    # Empty list means ALL dependencies are statically linked into the shared library
    dynamic_deps = [],
    # Additional linker options for dynamic library
    user_link_flags = select({
        ":linux_aarch64": [
            "-Wl,-soname,libscore_communication.so",
            "-Wl,-rpath,'$$ORIGIN'",  # Set RPATH for runtime library search
            "-Wl,--version-script=$(location :generate_version_script)",  # Symbol versioning
            "-Wl,--allow-multiple-definition",  # Allow duplicate symbols (choose first)
            "-lacl",  # Link against ACL library for file permissions (needed for aarch64 - Not pulling from Deps for some reason)
            "-static-libgcc",  # Statically link libgcc
            "-static-libstdc++",  # Statically link libstdc++
        ],
        ":linux_x86_64": [
            "-Wl,-soname,libscore_communication.so",
            "-Wl,-rpath,'$$ORIGIN'",  # Set RPATH for runtime library search
            "-Wl,--version-script=$(location :generate_version_script)",  # Symbol versioning
            "-Wl,--allow-multiple-definition",  # Allow duplicate symbols (choose first)
            "-static-libgcc",  # Statically link libgcc
            "-static-libstdc++",  # Statically link libstdc++
        ],
        "@platforms//os:macos": [
            "-Wl,-install_name,@rpath/libscore_communication.dylib",
            "-Wl,-rpath,@loader_path",
            "-static-libgcc",
            "-static-libstdc++",
        ],
        ":qnx7": [
            "-Wl,-soname,libscore_communication.so",
            "-Wl,-rpath,'$$ORIGIN'",
            "-Wl,--version-script=$(location :generate_version_script)",  # QNX supports GNU ld version scripts
            "-lslog2",  # QNX logging library (system library, must be dynamic - Not pulling from Deps for some reason)
            "-Wl,--allow-multiple-definition",  # Allow duplicate symbols (choose first)
            "-static-libgcc",
            "-static-libstdc++",
        ],
        ":qnx8": [
            "-Wl,-soname,libscore_communication.so",
            "-Wl,-rpath,'$$ORIGIN'",
            "-Wl,--version-script=$(location :generate_version_script)",  # QNX supports GNU ld version scripts
            "-lslog2",  # QNX logging library (system library, must be dynamic - Not pulling from Deps for some reason)
            "-lfsnotify",  # QNX8-specific: fsnotify library (system library, must be dynamic - Not pulling from Deps for some reason)
            "-Wl,--allow-multiple-definition",  # Allow duplicate symbols (choose first)
            "-static-libgcc",
            "-static-libstdc++",
        ],
        "//conditions:default": [],
    }),
    additional_linker_inputs = select({
        ":linux_aarch64": [":generate_version_script"],
        ":linux_x86_64": [":generate_version_script"],
        ":qnx7": [":generate_version_script"],
        ":qnx8": [":generate_version_script"],
        "//conditions:default": [],
    }),
)

# ============================================================================
# Extracting all Public Headers Needed for Package Deployment
# ============================================================================

# Extract headers from the cc_library dependencies We need headers to be collected.
# There is not proper way as most of the headers are not exported. Use genrule to pull them together
extract_headers(
    name = "score_com_public_headers",
    deps = [
        "//score/mw/com:com",
        "@score_baselibs//score/language/futurecpp",
        "@score_logging//score/mw/log",
    ],
)

# Copy headers into include/ directory with proper structure
copy_headers(
    name = "copied_headers",
    headers = ":score_com_public_headers",
)

# Filegroup to make copied headers visible to other packages
filegroup(
    name = "public_headers",
    srcs = [":copied_headers"],
    visibility = ["//visibility:public"],
)

# ============================================================================
# Create a cc_library Rule that imports Headers and Shared Library
# ============================================================================

# Import the shared library properly using cc_import
cc_import(
    name = "libscore_com_import",
    #shared_library = ":score_communication",   #Works & have all the symbols needed by example application
    shared_library = ":score_com",              #Do not work & Only symbols from API Layer inside - example application calls directly to Proxy. ProxyBase and other lower layer targets.
)

# Wrapper library that provides both the shared library and headers
# The headers are in include/ directory, so we strip that prefix
cc_library(
    name = "score_com_dynamic",
    hdrs = [":public_headers"],
    strip_include_prefix = "include",
    visibility = ["//visibility:public"],
    deps = [
        ":libscore_com_import",
    ],
)

# ================================================================================================
# Now Build the Application via Dynamic Path & Header Folders ( simulating the deployment package)
# ================================================================================================

cc_binary(
    name = "ipc_bridge_dynamic",
    srcs = [
        "//score/mw/com/example/ipc_bridge:assert_handler.cpp",
        "//score/mw/com/example/ipc_bridge:assert_handler.h",
        "//score/mw/com/example/ipc_bridge:datatype.cpp",
        "//score/mw/com/example/ipc_bridge:datatype.h",
        "//score/mw/com/example/ipc_bridge:sample_sender_receiver.cpp",
        "//score/mw/com/example/ipc_bridge:sample_sender_receiver.h",
        "//score/mw/com/example/ipc_bridge:main.cpp",
    ],
    data = ["//score/mw/com/example/ipc_bridge:etc/mw_com_config.json"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        "@boost.program_options",
        ":score_com_dynamic",  #Link dynamically against the shared library
    ],
    visibility = ["//visibility:public"],
)


# ================================================================================================
# TBD: Create a tarball package with the required libraries, TestAPP & Data files 
# ================================================================================================
