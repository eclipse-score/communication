load("//quality/integration_testing:integration_testing.bzl", "integration_test")
load("//score/mw/com/test:pkg_application.bzl", "pkg_application")

pkg_application(
    name = "logging_test_app",
    app_name = "someip_routing",
    bin = ["//score/someip/routing_module/iosock_bridge:mods_someip_routing"],
    etc = [
        "mw_com_config.json",
        "//score/someip/routing_module/iosock_bridge:logging.json",
    ],
    target_compatible_with = ["@platforms//os:qnx"],
)

integration_test(
    name = "logging_test",
    timeout = "moderate",
    srcs = ["logging_test.py"],
    filesystem = ":logging_test_app",
    target_compatible_with = ["@platforms//os:qnx"],
)
