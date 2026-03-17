# *******************************************************************************
# Copyright (c) 2025 Contributors to the Eclipse Foundation
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

load("@bazel_skylib//rules:expand_template.bzl", "expand_template")
load("@score_communication_pip//:requirements.bzl", "requirement")
load("@score_toolchains_qnx//rules/fs:ifs.bzl", "qnx_ifs")
load("//quality/qnx_unit_testing/environments:run_as_exec.bzl", "run_as_exec")

# Export Bazel macros and shell scripts for QNX unit testing
exports_files(
    [
        "cc_test_qnx.bzl",
        "py_unittest_qnx_test.bzl",
        "run_qemu_shell.sh",
        "run_qemu.sh",
        "configure_persistency.sh",
        "run_test.sh",
        "startup.sh",
        "tools.build",
        "init_x86_64_cc_test.build.template",
    ],
    visibility = ["//visibility:public"],
)

# Create empty FAT32 filesystem image for test results (10MB)
genrule(
    name = "test_results",
    outs = ["test_results.img"],
    cmd = """
        # Create 10MB empty file for FAT32 test result exchange
        dd if=/dev/zero of=$@ bs=1M count=10 2>/dev/null
    """,
    tags = ["manual"],
    visibility = ["//visibility:public"],
)

# Create empty QNX6 filesystem image for persistent storage (10MB)
genrule(
    name = "persistent",
    outs = ["persistent.img"],
    cmd = """
        # Create 10MB empty file for QNX6 persistent storage
        dd if=/dev/zero of=$@ bs=1M count=10 2>/dev/null
    """,
    tags = ["manual"],
    visibility = ["//visibility:public"],
)

# Generate IFS build file for C++ unit tests (runs test binary)
expand_template(
    name = "init_build_cc_test",
    out = "init_cc_test.build",
    substitutions = {
        "{RUN_BINARY}": "run_test.sh",
    },
    template = ":init_x86_64_cc_test.build.template",
    visibility = ["//visibility:public"],
)

# Generate IFS build file for interactive shell (debugging)
expand_template(
    name = "init_build_shell",
    out = "init_shell.build",
    substitutions = {
        "{RUN_BINARY}": "[+session] /bin/sh &",
    },
    template = ":init_x86_64_cc_test.build.template",
    visibility = ["//visibility:public"],
)

# QNX bootable image for interactive shell (debugging)
qnx_ifs(
    name = "init_shell",
    testonly = True,
    srcs = [
        ":configure_persistency.sh",
        ":run_test.sh",
        ":startup.sh",
        ":tools.build",
    ],
    build_file = ":init_build_shell",
    ext_repo_maping = {
        "CONFIGURE_PERSISTENCY_SCRIPT": "$(location :configure_persistency.sh)",
        "RUN_TEST_SCRIPT": "$(location :run_test.sh)",
        "STARTUP_SCRIPT": "$(location :startup.sh)",
        "TOOLS_BUILDFILE": "$(location :tools.build)",
    },
    tags = ["manual"],
    target_compatible_with = ["@platforms//os:qnx"],
    visibility = ["//visibility:public"],
)

# Python script to parse test results from FAT32 image
py_binary(
    name = "process_test_results_bin",
    srcs = ["process_test_results.py"],
    main = "process_test_results.py",
    tags = ["manual"],
    visibility = ["//visibility:public"],
    deps = [
        requirement("pyfatfs"),
        requirement("fs"),
    ],
)

# Wrap py_binary with run_as_exec to force execution platform (Linux)
# even when building with --config=qnx_x86_64 (which sets target platform to QNX)
run_as_exec(
    name = "process_test_results",
    executable = ":process_test_results_bin",
    visibility = ["//visibility:public"],
)
