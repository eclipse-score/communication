load("@score_baselibs//score/language/safecpp:toolchain_features.bzl", "COMPILER_WARNING_FEATURES")
load("//bazel/tools:json_schema_validator.bzl", "validate_json_schema_test")
load("//score/mw/com/test:pkg_application.bzl", "pkg_application")

validate_json_schema_test(
    name = "validate_mw_com_config_schema",
    json = "config/mw_com_config.json",
    schema = "//score/mw/com/impl/configuration:mw_com_config_schema",
    tags = ["lint"],
)

cc_library(
    name = "test_checkpoints",
    srcs = ["test_checkpoints.cpp"],
    hdrs = ["test_checkpoints.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//score/mw/com/test/partial_restart/methods:__subpackages__"],
)

cc_binary(
    name = "consumer_restart_application",
    srcs = [
        "consumer_restart_application.cpp",
        "consumer_restart_application.h",
    ],
    data = ["config/mw_com_config.json"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":controller",
        "//score/mw/com/test/common_test_resources:command_line_parser",
        "//score/mw/com/test/common_test_resources:stop_token_sig_term_handler",
        "//score/mw/com/test/common_test_resources:test_resources",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/mw/log",
    ],
)

cc_library(
    name = "consumer",
    srcs = ["consumer.cpp"],
    hdrs = ["consumer.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:test_resources",
        "//score/mw/com/test/partial_restart:consumer_handle_notification_data",
        "//score/mw/com/test/partial_restart/methods:test_method_datatype",
    ],
    visibility = ["//score/mw/com/test/partial_restart/methods:__subpackages__"],
    deps = [
        ":test_checkpoints",
        "//score/mw/com/test/common_test_resources:fail_test",
        "//score/mw/com/test/common_test_resources:test_resources",
    ],
)

cc_library(
    name = "provider",
    srcs = ["provider.cpp"],
    hdrs = ["provider.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        "//score/mw/com",
        "//score/mw/com/test/partial_restart/methods:test_method_datatype",
    ],
    visibility = ["//score/mw/com/test/partial_restart/methods:__subpackages__"],
    deps = [
        ":test_checkpoints",
        "//score/mw/com/test/common_test_resources:fail_test",
        "//score/mw/com/test/common_test_resources:test_resources",
    ],
)

cc_library(
    name = "controller",
    srcs = ["controller.cpp"],
    hdrs = ["controller.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:fail_test",
        "//score/mw/com/test/common_test_resources:test_resources",
    ],
    visibility = ["//score/mw/com/test/partial_restart/methods:__subpackages__"],
    deps = [
        ":consumer",
        ":provider",
        ":test_checkpoints",
        "//score/mw/com/test/common_test_resources:shared_memory_object_guard",
        "@score_baselibs//score/language/futurecpp",
    ],
)

pkg_application(
    name = "consumer_restart-pkg",
    app_name = "consumer_restart_methods",
    bin = [":consumer_restart_application"],
    etc = [
        "config/mw_com_config.json",
        "config/logging.json",
    ],
    visibility = [
        "//score/mw/com/test/partial_restart/methods/consumer_restart/integration_test:__subpackages__",
    ],
)
