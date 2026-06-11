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

# Clang warning features.
# Diagnostics reference: https://clang.llvm.org/docs/DiagnosticsReference.html

load("@rules_cc//cc/toolchains:args.bzl", "cc_args")
load("@rules_cc//cc/toolchains:feature.bzl", "cc_feature")

# Minimal warning baseline for production code.

cc_args(
    name = "minimal_warnings_args",
    actions = [
        "@rules_cc//cc/toolchains/actions:compile_actions",
    ],
    args = [
        # Suppress false positive from Clang's self-assignment overloaded check.
        # See https://bugs.llvm.org/show_bug.cgi?id=43124
        "-Wno-error=self-assign-overloaded",
    ],
)

cc_feature(
    name = "minimal_warnings",
    args = [
        "//quality/compiler_warnings:minimal_warnings_args",
        ":minimal_warnings_args",
    ],
    feature_name = "score_communication_minimal_warnings",
    visibility = ["//visibility:public"],
)

cc_args(
    name = "strict_warnings_args",
    actions = [
        "@rules_cc//cc/toolchains/actions:compile_actions",
    ],
    args = [
        "-Wfloat-equal",
        "-Wformat-security",
        "-Wshadow-all",
        # Demote shadow-uncaptured-local from error to warning. This diagnostic
        # fires for code in external headers (e.g. score_baselibs simple_task.h)
        # which we cannot fix.
        # raised a ticket in score_baselibs to fix this issue, but for now we demote it to warning
        # Bug ticket: (https://github.com/eclipse-score/baselibs/issues/304)
        "-Wno-error=shadow-uncaptured-local",
    ],
)

cc_feature(
    name = "strict_warnings",
    args = [
        "//quality/compiler_warnings:strict_warnings_args",
        ":strict_warnings_args",
    ],
    feature_name = "score_communication_strict_warnings",
    implies = ["minimal_warnings"],
    visibility = ["//visibility:public"],
)

cc_args(
    name = "strict_warnings_no_error_args",
    actions = [
        "@rules_cc//cc/toolchains/actions:compile_actions",
    ],
    args = [
        "-Wno-error",
    ],
)

cc_feature(
    name = "strict_warnings_no_error",
    args = [
        ":strict_warnings_no_error_args",
    ],
    feature_name = "score_communication_strict_warnings_no_error",
    implies = ["strict_warnings"],
    visibility = ["//visibility:public"],
)

# Third-party profile: suppress warnings for external code only.

cc_args(
    name = "third_party_warnings_args",
    actions = [
        "@rules_cc//cc/toolchains/actions:compile_actions",
    ],
    args = [
        # Suppress all warnings. Used for third-party code where we cannot
        # fix upstream warnings and they would only add noise.
        "--no-warnings",
        # NOTE: Some diagnostics are by default an error in clang and not a warning.
        # Thus, `--no-warnings` cannot silence them. Such diagnostics have to be
        # explicitly disabled via `-Wno-error=`, as done for the following ones:
        # due to https://releases.llvm.org/16.0.0/tools/clang/docs/ReleaseNotes.html#potentially-breaking-changes
        "-Wno-error=implicit-function-declaration",
        # since -Wmissing-template-arg-list-after-template-kw (cf. https://github.com/llvm/llvm-project/pull/80801)
        # is an error by default starting from clang 19.1.0 on
        # (cf. https://releases.llvm.org/19.1.0/tools/clang/docs/DiagnosticsReference.html#wmissing-template-arg-list-after-template-kw)
        # regardless of the --no-warnings flag, we demote such warning to no-error for all our third-party code
        "-Wno-error=missing-template-arg-list-after-template-kw",
    ],
)

cc_feature(
    name = "third_party_warnings",
    args = [
        ":third_party_warnings_args",
    ],
    feature_name = "score_communication_third_party_warnings",
    mutually_exclusive = [
        "//quality/compiler_warnings:additional_warnings",
        "//quality/compiler_warnings:treat_warnings_as_errors",
        ":minimal_warnings",
        ":strict_warnings",
    ],
    visibility = ["//visibility:public"],
)
