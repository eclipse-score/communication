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

load("@score_tooling//bazel/rules/rules_score:rules_score.bzl", "unit_design")
load("//tools/documentation:defs.bzl", "html_integrity_test", "plantuml_render")

unit_design(
    name = "client_connection_unit_design",
    dynamic = [
        "client_connection_connect_activity.puml",
        "client_connection_endpoint_callbacks_activity.puml",
        "client_connection_lifetime_operation_activity.puml",
        "client_connection_lifetime_retirement_activity.puml",
        "client_connection_lifetime_transaction_activity.puml",
        "client_connection_object_lifetime_activity.puml",
        "client_connection_process_input_event_activity.puml",
        "client_connection_send_activity.puml",
        "client_connection_send_wait_reply_activity.puml",
        "client_connection_send_with_callback_activity.puml",
        "client_connection_stop_activity.puml",
        "client_connection_switch_to_stop_state_activity.puml",
        "client_send_queue_arm_activity.puml",
        "client_send_queue_cancel_activity.puml",
        "client_send_queue_drain_activity.puml",
        "client_send_queue_handle_reply_activity.puml",
        "client_send_queue_send_activity.puml",
        "client_send_queue_send_wait_reply_activity.puml",
        "client_send_queue_send_with_callback_activity.puml",
        "client_send_queue_try_queue_activity.puml",
    ],
    static = [
        "client_connection.rst",
        "client_connection_class_diagram.puml",
    ],
    visibility = ["//score/message_passing:__pkg__"],
)

plantuml_render(
    name = "client_connection_render",
    srcs = glob([
        "client_connection*.puml",
        "client_send_queue*.puml",
    ]),
)

html_integrity_test(
    name = "client_connection_html_test",
    size = "small",
    expectations = "client_connection_html_expectations.json",
    site = "//docs/sphinx:sphinx_doc",
    tags = ["manual"],
    target_compatible_with = ["@platforms//os:linux"],
)

unit_design(
    name = "unix_domain_client_factory_unit_design",
    visibility = ["//score/message_passing:__pkg__"],
)

unit_design(
    name = "qnx_dispatch_client_factory_unit_design",
    visibility = ["//score/message_passing:__pkg__"],
)

unit_design(
    name = "unix_domain_server_unit_design",
    visibility = ["//score/message_passing:__pkg__"],
)

unit_design(
    name = "qnx_dispatch_server_unit_design",
    visibility = ["//score/message_passing:__pkg__"],
)

unit_design(
    name = "unix_domain_server_factory_unit_design",
    visibility = ["//score/message_passing:__pkg__"],
)

unit_design(
    name = "qnx_dispatch_server_factory_unit_design",
    visibility = ["//score/message_passing:__pkg__"],
)

unit_design(
    name = "unix_domain_engine_unit_design",
    visibility = ["//score/message_passing:__pkg__"],
)

unit_design(
    name = "qnx_dispatch_engine_unit_design",
    visibility = ["//score/message_passing:__pkg__"],
)
