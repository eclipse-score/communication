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

load("//quality/unit_testing:unit_testing.bzl", "cc_unit_test")
load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")

cc_unit_test(
    name = "event_data_storage_test",
    srcs = ["event_data_storage_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        "//score/mw/com/impl/bindings/someip:event_data_storage",
        "@score_baselibs//score/memory:data_type_size_info",
    ],
)

cc_unit_test(
    name = "slot_allocation_control_test",
    srcs = ["slot_allocation_control_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = ["//score/mw/com/impl/bindings/someip:slot_allocation_control"],
)

cc_unit_test(
    name = "in_process_transport_test",
    srcs = ["in_process_transport_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        "//score/mw/com/impl/bindings/someip:transport",
    ],
)

cc_unit_test(
    name = "skeleton_event_test",
    srcs = ["skeleton_event_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        "//score/mw/com/impl:instance_identifier",
        "//score/mw/com/impl/bindings/someip:sample_allocatee_ptr",
        "//score/mw/com/impl/bindings/someip:skeleton",
        "//score/mw/com/impl/bindings/someip:skeleton_event",
        "//score/mw/com/impl/bindings/someip:transport",
        "//score/mw/com/impl/configuration",
    ],
)
