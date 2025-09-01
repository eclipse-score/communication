load("@score-baselibs//:bazel/unit_tests.bzl", "cc_unit_test_suites_for_host_and_qnx")

cc_unit_test_suites_for_host_and_qnx(
    name = "unit_tests",
    test_suites_from_sub_packages = [
        "//score/mw/com:unit_test_suite",
        # "@score-baselibs//score/mw/log:unit_tests",
    ],
    visibility = [
        "//visibility:public",
    ],
)
