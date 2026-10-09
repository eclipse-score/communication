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

load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")
load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")

# Tests for //score/mw/com/impl:serializer_manager. The manager dlopens the generated serialization
# library by soname, and the dynamic loader's environment is fixed once per process, so the success,
# load-failure and symbol-failure scenarios cannot share a process. All test cases live in the single
# serializer_manager_test.cpp; the three targets below compile that file and each run only the matching
# test suite (via --gtest_filter) under the runtime environment that scenario requires.

# Loads the real generated serialization library via dlopen: the .so is provided as a data dependency
# and made discoverable to the dynamic loader through LD_LIBRARY_PATH (relative to the runfiles root), so
# dlopen and dlsym both succeed. Uses cc_test rather than cc_unit_test because the latter wraps the test
# and drops the "env" attribute.
cc_test(
    name = "serializer_manager_test",
    size = "small",
    timeout = "short",
    srcs = ["serializer_manager_test.cpp"],
    args = ["--gtest_filter=SerializerManagerTest.*"],
    data = ["//score/serialization/someip/generated_code_example:libmwcom_serializer_someip.so"],
    env = {
        "LD_LIBRARY_PATH": "score/serialization/someip/generated_code_example",
    },
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    deps = [
        "//score/mw/com/impl:serializer_manager",
        "//score/serialization/someip:serializer",
        "//score/serialization/someip/generated_code_example:speed_rpm",
        "@googletest//:gtest_main",
        "@score_baselibs//score/result",
    ],
)

# Complements serializer_manager_test: intentionally omits the generated .so and LD_LIBRARY_PATH so that
# the manager's dlopen fails, exercising the load-failure branch (SerializerErrc::kCreationFailed).
cc_test(
    name = "serializer_manager_load_failure_test",
    size = "small",
    timeout = "short",
    srcs = ["serializer_manager_test.cpp"],
    args = ["--gtest_filter=SerializerManagerLoadFailureTest.*"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    deps = [
        "//score/mw/com/impl:serializer_manager",
        "//score/serialization/someip:serializer",
        "//score/serialization/someip/generated_code_example:speed_rpm",
        "@googletest//:gtest_main",
        "@score_baselibs//score/result",
    ],
)

# Stub serialization library that carries the expected soname (libmwcom_serializer_someip.so) but exports
# none of the factory symbols, so that the manager's dlopen succeeds while dlsym fails. Used only by
# serializer_manager_symbol_failure_test below. The trivial source is generated inline to avoid an extra
# standalone .cpp; the library must still be a separate .so because the manager reaches it via dlopen.
genrule(
    name = "stub_serializer_missing_symbols_src",
    outs = ["stub_serializer_missing_symbols.cpp"],
    cmd = "echo 'extern \"C\" int mwcom_serializer_stub_marker() { return 0; }' > $@",
)

cc_binary(
    name = "libmwcom_serializer_someip.so",
    testonly = True,
    srcs = [":stub_serializer_missing_symbols_src"],
    features = COMPILER_WARNING_FEATURES,
    linkshared = True,
    tags = ["FFI"],
)

# Makes dlopen succeed but dlsym fail by putting the symbol-less stub library (above) on the loader's
# search path under the expected soname, exercising the symbol-resolution failure branch
# (SerializerErrc::kCreationFailed after dlopen succeeds).
cc_test(
    name = "serializer_manager_symbol_failure_test",
    size = "small",
    timeout = "short",
    srcs = ["serializer_manager_test.cpp"],
    args = ["--gtest_filter=SerializerManagerSymbolFailureTest.*"],
    data = [":libmwcom_serializer_someip.so"],
    env = {
        "LD_LIBRARY_PATH": "score/mw/com/impl/test",
    },
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    deps = [
        "//score/mw/com/impl:serializer_manager",
        "//score/serialization/someip:serializer",
        "//score/serialization/someip/generated_code_example:speed_rpm",
        "@googletest//:gtest_main",
        "@score_baselibs//score/result",
    ],
)

cc_library(
    name = "binding_factory_resources",
    testonly = True,
    srcs = ["binding_factory_resources.cpp"],
    hdrs = ["binding_factory_resources.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/gateway:__subpackages__",
        "//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        "//score/mw/com/impl",
        "//score/mw/com/impl/bindings/mock_binding:mock_binding_lib",
        "//score/mw/com/impl/plumbing:proxy_binding_factory_mock",
        "//score/mw/com/impl/plumbing:proxy_event_binding_factory_mock",
        "//score/mw/com/impl/plumbing:proxy_field_binding_factory_mock",
        "//score/mw/com/impl/plumbing:proxy_method_binding_factory_mock",
        "//score/mw/com/impl/plumbing:skeleton_binding_factory_mock",
        "//score/mw/com/impl/plumbing:skeleton_event_binding_factory_mock",
        "//score/mw/com/impl/plumbing:skeleton_field_binding_factory_mock",
        "//score/mw/com/impl/plumbing:skeleton_method_binding_factory_mock",
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "proxy_resources",
    testonly = True,
    srcs = ["proxy_resources.cpp"],
    hdrs = ["proxy_resources.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//score/mw/com/impl:__subpackages__"],
    deps = [
        "//score/mw/com/impl",
        "//score/mw/com/impl/bindings/mock_binding:mock_binding_lib",
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "dummy_instance_identifier_builder",
    testonly = True,
    srcs = ["dummy_instance_identifier_builder.cpp"],
    hdrs = ["dummy_instance_identifier_builder.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = ["@score_baselibs//score/language/futurecpp"],
    visibility = [
        "//score/mw/com/gateway:__subpackages__",
        "//score/mw/com/impl:__subpackages__",
    ],
    deps = ["//score/mw/com/impl"],
)

cc_library(
    name = "runtime_mock_guard",
    testonly = True,
    srcs = ["runtime_mock_guard.cpp"],
    hdrs = ["runtime_mock_guard.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [],
    visibility = ["//score/mw/com:__subpackages__"],
    deps = [
        "//score/mw/com/impl:runtime",
        "//score/mw/com/impl:runtime_mock",
        "@googletest//:gtest",
    ],
)
