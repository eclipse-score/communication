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

"""
QNX C++ Unit Test Macro for SCORE Integration Testing Framework

This macro wraps cc_test targets to run them on QNX 8.0 in QEMU using the
SCORE integration testing framework's QEMURunner infrastructure.

It creates a bootable QNX image containing the test binary and executes it
using Python-based QEMU management (replacing the old shell-based approach).

## Architecture:

1. **qnx_ifs target**: Creates QNX bootable .ifs image
   - Marked with target_compatible_with = QNX
   - Built with QNX cross-compilation toolchain (--config=qnx_x86_64)

2. **py_test wrapper**: Runs on Linux host
   - Uses QEMURunner from quality/integration_testing/environments/qnx8_qemu/
   - Launches QEMU, executes tests, parses results from FAT32 image
   - No target_compatible_with (runs on execution platform)

This consolidates QEMU logic into a single Python-based implementation,
eliminating the previous duplication between shell scripts and Python.
"""

load("@rules_pkg//pkg:mappings.bzl", "pkg_attributes", "pkg_files")
load("@rules_pkg//pkg:tar.bzl", "pkg_tar")
load("@score_toolchains_qnx//rules/fs:ifs.bzl", "qnx_ifs")

def _get_test_and_data_impl(ctx):
    """Extract test binary and data files (excluding .so libraries)"""
    data_files = []
    for file in ctx.attr.src[DefaultInfo].data_runfiles.files.to_list():
        if not file.basename.endswith(".so"):
            data_files.append(file)
    return [DefaultInfo(files = ctx.attr.src[DefaultInfo].files, runfiles = ctx.runfiles(files = data_files))]

_get_test_and_data = rule(
    implementation = _get_test_and_data_impl,
    attrs = {
        "src": attr.label(providers = [CcInfo]),
    },
)

def cc_test_qnx(name, cc_test, excluded_tests_filter = None, app_tar = None):
    """Compile and run C++ unit tests on QNX 8.0 in QEMU

    This macro uses the SCORE integration testing framework's QEMURunner
    infrastructure to execute C++ tests in a QNX virtual machine.

    Args:
      name: Test name
      cc_test: cc_test target to execute in QNX
      excluded_tests_filter: list of GTest filters to exclude from execution.
        Examples:
          ["FooTest.Test1"] - do not run Test1 from test suite FooTest
          ["FooTest.*"] - do not run any test from test suite FooTest
          ["*FooTest.*"] - runs all non-FooTest tests
      app_tar: optional pkg_tar target containing application binaries to include
        in the QNX image. Similar to integration_test's filesystem parameter.
        The tar will be extracted to /app directory in the QNX filesystem.
    """
    excluded_tests_filter = excluded_tests_filter if excluded_tests_filter else []

    # Convert filter list to GTest format: "-FooTest.Test1:BarTest.*"
    excluded_tests_filter_str = "-"
    for test_filter in excluded_tests_filter:
        excluded_tests_filter_str = excluded_tests_filter_str + (test_filter + ":\\")

    native.genrule(
        name = "{}_excluded_tests_filter".format(name),
        cmd_bash = """
        echo {} > $(@)
        """.format(excluded_tests_filter_str),
        testonly = True,
        tags = ["manual"],
        outs = ["{}_excluded_tests_filter.txt".format(name)],
    )

    _get_test_and_data(
        name = "%s_test_and_data" % name,
        src = cc_test,
        testonly = True,
        tags = ["manual"],
    )

    pkg_files(
        name = "%s_test_and_runfiles" % name,
        srcs = [
            ":{}_excluded_tests_filter".format(name),
            ":{}_test_and_data".format(name),
        ],
        include_runfiles = True,
        prefix = "/tests",
        testonly = True,
        tags = ["manual"],
        attributes = pkg_attributes(mode = "0755"),
    )

    pkg_tar(
        name = "%s_pkgtar" % name,
        srcs = [
            "%s_test_and_runfiles" % name,
        ],
        testonly = True,
        tags = ["manual"],
        symlinks = {
            "/tests/cc_test_qnx": native.package_relative_label(cc_test).name,
            "/tests/cc_test_qnx_filters.txt": "{}_excluded_tests_filter.txt".format(name),
        },
        modes = {
            "/tests/cc_test_qnx": "0777",
            "/tests/cc_test_qnx_filters.txt": "0777",
        },
    )

    # Create QNX bootable image with test package
    # Uses tars parameter (like integration_test) instead of DUI
    # Optionally includes app_tar if provided (similar to integration_test's filesystem)
    qnx_ifs(
        name = "%s_test_img" % name,
        out = "{}_test.ifs".format(name),
        build_file = "//quality/qnx_unit_testing:init_build_cc_test",
        tars = {"TESTS": ":%s_pkgtar" % name} | ({"APP": app_tar} if app_tar else {}),
        testonly = True,
        target_compatible_with = [
            "@platforms//os:qnx",
        ],
        tags = [
            "manual",
        ],
    )

    # Shell test wrapper (temporary - keeps old sh_test approach for compatibility)
    # TODO: Migrate to py_test + QEMURunner once platform constraints are resolved
    native.sh_test(
        name = name,
        srcs = ["//quality/qnx_unit_testing:run_qemu.sh"],
        args = [
            "$(location //quality/qnx_unit_testing:init_shell)",
            "$(location //quality/qnx_unit_testing:persistent)",
            "$(location //quality/qnx_unit_testing:process_test_results)",
            "$(location //quality/qnx_unit_testing:test_results)",
            "$(location :%s_test_img)" % name,
        ],
        data = [
            ":%s_test_img" % name,
            "//quality/qnx_unit_testing:init_shell",
            "//quality/qnx_unit_testing:persistent",
            "//quality/qnx_unit_testing:process_test_results",
            "//quality/qnx_unit_testing:test_results",
        ],
        timeout = "short",
        size = "medium",
        # Note: sh_test runs on Linux host (launches QEMU), so no target_compatible_with
        # The QNX IFS image (qnx_ifs) has proper platform constraints
        tags = [
            "cpu:2",
            "manual",
            "qnx_unit_test",
        ],
    )

    # Shell access for debugging (kept for compatibility)
    native.sh_binary(
        name = "%s_shell" % name,
        srcs = ["//quality/qnx_unit_testing:run_qemu_shell.sh"],
        args = [
            "$(location //quality/qnx_unit_testing:init_shell)",
            "$(location //quality/qnx_unit_testing:persistent)",
            "$(location //quality/qnx_unit_testing:test_results)",
            "$(location :%s_test_img)" % name,
        ],
        data = [
            ":%s_test_img" % name,
            "//quality/qnx_unit_testing:init_shell",
            "//quality/qnx_unit_testing:persistent",
            "//quality/qnx_unit_testing:test_results",
        ],
        testonly = True,
        # Note: sh_binary runs on Linux host (launches QEMU), so no target_compatible_with
        tags = ["manual"],
    )
