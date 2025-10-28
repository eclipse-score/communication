load("@score_baselibs//:bazel/unit_tests.bzl", "cc_gtest_unit_test", "cc_unit_test", "cc_unit_test_suites_for_host_and_qnx")
load("@score_baselibs//score/language/safecpp:toolchain_features.bzl", "COMPILER_WARNING_FEATURES")

cc_gtest_unit_test(
    name = "offset_ptr_test",
    srcs = [
        "assignment_test.cpp",
        "bool_ops_test.cpp",
        "compare_test.cpp",
        "construction_test.cpp",
        "index_dereference_test.cpp",
        "misc_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "@score_baselibs//score/memory:__pkg__",
    ],
    deps = [
        ":offset_ptr_test_resources",
        "@score_baselibs//score/memory/shared:offset_ptr",
        "@score_baselibs//score/mw/log:backend_stub_testutil",
    ],
)

cc_unit_test(
    name = "offset_ptr_precondition_violation_test",
    srcs = [
        "arithmetic_test.cpp",
        "bounds_check_test.cpp",
        "copying_offset_ptr_bounds_check_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES + [
        # These tests catch exceptions instead of using gtest EXPECT_DEATH so we disable aborts_upon_exception.
        "-aborts_upon_exception",
    ],
    visibility = [
        "@score_baselibs//score/memory:__pkg__",
    ],
    deps = [
        ":bounds_check_memory_pool",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
        "@score_baselibs//score/language/futurecpp:futurecpp_test_support",
        "@score_baselibs//score/language/safecpp/coverage_termination_handler",
        "@score_baselibs//score/memory/shared:offset_ptr",
        "@score_baselibs//score/mw/log:backend_stub_testutil",
    ],
)

cc_library(
    name = "offset_ptr_test_resources",
    testonly = True,
    srcs = ["offset_ptr_test_resources.cpp"],
    hdrs = ["offset_ptr_test_resources.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["@score_baselibs//score/memory/shared:__pkg__"],
    deps = [
        "@googletest//:gtest_main",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/memory/shared",
        "@score_baselibs//score/memory/shared:fake_memory_resources",
    ],
)

cc_library(
    name = "bounds_check_memory_pool",
    testonly = True,
    srcs = ["bounds_check_memory_pool.cpp"],
    hdrs = ["bounds_check_memory_pool.h"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":offset_ptr_test_resources",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/memory/shared",
    ],
)

cc_unit_test_suites_for_host_and_qnx(
    name = "unit_test_suite",
    cc_unit_tests = [
        ":offset_ptr_precondition_violation_test",
        ":offset_ptr_test",
    ],
    visibility = [
        "@score_baselibs//score/memory/shared:__pkg__",
    ],
)
