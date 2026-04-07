test_suite(
    name = "component_tests",
    tests = [
        "//score/mw/com/test/partial_restart/methods/consumer_restart/integration_test:partial_restart_methods_consumer_graceful_test",
        "//score/mw/com/test/partial_restart/methods/consumer_restart/integration_test:partial_restart_methods_consumer_kill_test",
        "//score/mw/com/test/partial_restart/methods/consumer_restart/integration_test:partial_restart_methods_consumer_method_reenable_test",
        "//score/mw/com/test/partial_restart/methods/provider_restart/integration_test:partial_restart_methods_provider_graceful_no_proxy_test",
        "//score/mw/com/test/partial_restart/methods/provider_restart/integration_test:partial_restart_methods_provider_graceful_test",
        "//score/mw/com/test/partial_restart/methods/provider_restart/integration_test:partial_restart_methods_provider_kill_no_proxy_test",
        "//score/mw/com/test/partial_restart/methods/provider_restart/integration_test:partial_restart_methods_provider_kill_test",
    ],
    visibility = ["//score/mw/com/test:__pkg__"],
)

cc_library(
    name = "test_method_datatype",
    srcs = ["test_method_datatype.cpp"],
    hdrs = ["test_method_datatype.h"],
    visibility = ["//score/mw/com/test/partial_restart/methods:__subpackages__"],
    deps = [
        "//score/mw/com",
    ],
)

cc_library(
    name = "consumer_handle_notification_data",
    srcs = ["consumer_handle_notification_data.cpp"],
    hdrs = ["consumer_handle_notification_data.h"],
    visibility = ["//score/mw/com/test/partial_restart/methods:__subpackages__"],
    deps = [
        ":test_method_datatype",
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:check_point_control",
    ],
)
