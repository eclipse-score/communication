load("@rules_cc//cc:defs.bzl", "cc_library")
load("@score_baselibs//:bazel/unit_tests.bzl", "cc_gtest_unit_test", "cc_unit_test_suites_for_host_and_qnx")
load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")

cc_library(
    name = "test_type_utilities",
    srcs = ["test_type_utilities.cpp"],
    hdrs = ["test_type_utilities.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        "//score/mw/com:runtime",
    ],
    tags = ["FFI"],
    visibility = [
        "//score/mw/com:__subpackages__",
    ],
    deps = [
        ":i_runtime",
    ],
)

cc_library(
    name = "i_runtime",
    srcs = ["i_runtime.cpp"],
    hdrs = ["i_runtime.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com:__subpackages__",
    ],
    deps = [
        "//score/mw/com:runtime_configuration",
        "//score/mw/com:types",
        "//score/mw/com/impl:instance_specifier",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/memory:string_literal",
        "@score_baselibs//score/result",
    ],
)

cc_library(
    name = "runtime_mock",
    testonly = True,
    srcs = ["runtime_mock.cpp"],
    hdrs = ["runtime_mock.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        ":i_runtime",
        "@googletest//:gtest",
    ],
)

cc_gtest_unit_test(
    name = "runtime_mock_test",
    srcs = ["runtime_mock_test.cpp"],
    deps = [
        ":runtime_mock",
        ":test_type_utilities",
        "//score/mw/com:runtime",
        "//score/mw/com/impl/mocking:test_type_utilities",
    ],
)

cc_unit_test_suites_for_host_and_qnx(
    name = "unit_test_suite",
    cc_unit_tests = [
        ":runtime_mock_test",
    ],
    test_suites_from_sub_packages = [],
    visibility = ["//score/mw/com:__pkg__"],
)
