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

load("@rules_pkg//pkg:mappings.bzl", "pkg_filegroup")
load("//quality/integration_testing:integration_testing.bzl", "integration_test")

integration_test(
    name = "move_construct_before_offered_skeleton_same_process_test",
    srcs = [
        "move_construct_before_offered_skeleton_same_process_test.py",
        "test_fixture.py",
    ],
    filesystem = "//score/mw/com/test/move_semantics/skeleton_method:main_consumer_and_provider-pkg",
)

integration_test(
    name = "move_construct_after_offered_skeleton_same_process_test",
    srcs = [
        "move_construct_after_offered_skeleton_same_process_test.py",
        "test_fixture.py",
    ],
    filesystem = "//score/mw/com/test/move_semantics/skeleton_method:main_consumer_and_provider-pkg",
)

integration_test(
    name = "move_assign_before_offered_skeleton_same_process_test",
    srcs = [
        "move_assign_before_offered_skeleton_same_process_test.py",
        "test_fixture.py",
    ],
    filesystem = "//score/mw/com/test/move_semantics/skeleton_method:main_consumer_and_provider-pkg",
)

integration_test(
    name = "move_assign_after_offered_skeleton_same_process_test",
    srcs = [
        "move_assign_after_offered_skeleton_same_process_test.py",
        "test_fixture.py",
    ],
    filesystem = "//score/mw/com/test/move_semantics/skeleton_method:main_consumer_and_provider-pkg",
)

pkg_filegroup(
    name = "different_processes_filesystem",
    srcs = [
        "//score/mw/com/test/move_semantics/skeleton_method:main_consumer-pkg",
        "//score/mw/com/test/move_semantics/skeleton_method:main_provider-pkg",
    ],
)

integration_test(
    name = "move_construct_before_offered_skeleton_different_process_test",
    srcs = [
        "move_construct_before_offered_skeleton_different_process_test.py",
        "test_fixture.py",
    ],
    filesystem = ":different_processes_filesystem",
)

integration_test(
    name = "move_construct_after_offered_skeleton_different_process_test",
    srcs = [
        "move_construct_after_offered_skeleton_different_process_test.py",
        "test_fixture.py",
    ],
    filesystem = ":different_processes_filesystem",
)

integration_test(
    name = "move_assign_before_offered_skeleton_different_process_test",
    srcs = [
        "move_assign_before_offered_skeleton_different_process_test.py",
        "test_fixture.py",
    ],
    filesystem = ":different_processes_filesystem",
)

integration_test(
    name = "move_assign_after_offered_skeleton_different_process_test",
    srcs = [
        "move_assign_after_offered_skeleton_different_process_test.py",
        "test_fixture.py",
    ],
    filesystem = ":different_processes_filesystem",
)
