# *******************************************************************************
# Copyright (c) 2026 Contributors to the Eclipse Foundation
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

load("@rules_cc//cc:defs.bzl", "cc_binary")
load("@rules_pkg//pkg:mappings.bzl", "pkg_files")
load("//quality/integration_testing:integration_testing.bzl", "integration_test")
load("//score/mw/com/test:pkg_application.bzl", "pkg_application")

cc_binary(
    name = "rx_probe",
    srcs = ["rx_probe.cpp"],
    linkopts = select({
        "@platforms//os:qnx": [
            "-lsocket",
            "-lfsnotify",
        ],
        "//conditions:default": [],
    }),
    deps = [
        "//score/message_passing",
        "//score/mw/com",
        "//score/someip/routing_control_protocol",
        "//score/someip/routing_module/lola_mapping",
        "@score_baselibs//score/filesystem",
    ] + select({
        "@platforms//os:qnx": ["@score_logging//score/mw/log/backend:slog"],
        "//conditions:default": [],
    }),
)

pkg_files(
    name = "linux_logging_config",
    srcs = ["linux_logging.json"],
    renames = {"linux_logging.json": "logging.json"},
    target_compatible_with = ["@platforms//os:linux"],
)

pkg_application(
    name = "linux_rx_test_app",
    app_name = "someip_routing",
    bin = [
        ":rx_probe",
        "//score/someip/routing_module/udp_transport:linux_routing_module",
    ],
    etc = ["mw_com_config.json"],
    etc_deps = [":linux_logging_config"],
    target_compatible_with = ["@platforms//os:linux"],
)

pkg_application(
    name = "qnx_rx_test_app",
    app_name = "someip_routing",
    bin = [
        ":rx_probe",
        "//score/someip/routing_module/routing_bridge:mods_someip_routing",
    ],
    etc = [
        "logging.json",
        "mw_com_config.json",
    ],
    target_compatible_with = ["@platforms//os:qnx"],
)

RX_SCENARIOS = [
    ("basic_udp_notification_test", "test_basic_udp_notification"),
    ("unknown_event_test", "test_unknown_event_is_not_forwarded"),
    ("wrong_sender_port_test", "test_wrong_sender_port_is_not_forwarded"),
    ("wrong_local_port_test", "test_wrong_local_port_is_not_forwarded"),
    ("wrong_service_test", "test_wrong_service_is_not_forwarded"),
    ("non_notification_test", "test_non_notification_is_not_forwarded"),
    ("malformed_notification_test", "test_malformed_notification_is_not_forwarded"),
    ("oversized_notification_test", "test_oversized_notification_is_not_forwarded"),
    ("concatenated_notifications_test", "test_concatenated_notifications_are_delivered_in_order"),
    ("removed_subscription_test", "test_removed_subscription_stops_delivery"),
    ("removed_remote_service_test", "test_removed_remote_service_stops_delivery"),
    ("controller_disconnect_test", "test_controller_disconnect_removes_routing_state"),
]

[
    integration_test(
        name = "linux_" + name,
        timeout = "moderate",
        srcs = ["linux_rx_test.py"],
        args = [
            "-k",
            test_case,
        ],
        data = ["qnx_rx_test.py"],
        filesystem = ":linux_rx_test_app",
        target_compatible_with = ["@platforms//os:linux"],
    )
    for name, test_case in RX_SCENARIOS + [("startup_test", "test_linux_routing_process_starts")]
]

test_suite(
    name = "linux_rx_test",
    tests = [":linux_" + name for name, _ in RX_SCENARIOS] + [":linux_startup_test"],
)

[
    integration_test(
        name = "qnx_" + name,
        timeout = "moderate",
        srcs = ["qnx_rx_test.py"],
        args = [
            "-k",
            test_case,
        ],
        filesystem = ":qnx_rx_test_app",
        tags = ["exclusive"],
        target_compatible_with = ["@platforms//os:qnx"],
    )
    for name, test_case in RX_SCENARIOS + [("logging_test", "test_module_loads_and_logs_to_slog")]
]

test_suite(
    name = "qnx_rx_test",
    tests = [":qnx_" + name for name, _ in RX_SCENARIOS] + [":qnx_logging_test"],
)
