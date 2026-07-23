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

# Common compiler warning features shared across GCC and Clang.

load("@rules_cc//cc/toolchains:args.bzl", "cc_args")
load("@rules_cc//cc/toolchains:feature.bzl", "cc_feature")

cc_args(
    name = "default_compile_args",
    actions = [
        "@rules_cc//cc/toolchains/actions:compile_actions",
    ],
    args = [
        "-march=nehalem",
        "-fstack-protector",
        "-fno-omit-frame-pointer",
        "-fPIC",
        "-fstack-protector-strong",
        "-fdiagnostics-color=always",
        # Fortify libc calls (buffer-overflow checks) used by message (de)serialization.
        # NOTE: effective only at -O1+; requires the non-executable stack link hardening below.
        "-D_FORTIFY_SOURCE=2",
        # Bounds/precondition checks on libstdc++ containers (ABI-safe; not _GLIBCXX_DEBUG).
        "-D_GLIBCXX_ASSERTIONS",
        # Per-page stack probing so a large VLA/alloca in deep IPC dispatch cannot skip the guard page.
        "-fstack-clash-protection",
        # x86-64 control-flow integrity (Intel CET: indirect-branch tracking + shadow stack); pairs with -march above.
        "-fcf-protection=full",
        # Mark emitted objects' stack non-executable (complements -Wl,-z,noexecstack in the link defaults).
        "-Wa,--noexecstack",
        "-D_QNX_SOURCE",
        "-D_XOPEN_SOURCE=700",  # SUSv4 (POSIX.1-2008 base specification plus the XSI extension)
        "-ffunction-sections",
        "-fdata-sections",
        "-no-canonical-prefixes",
        "-pthread",
    ],
    visibility = ["//quality/compiler_warnings:__subpackages__"],
)

cc_args(
    name = "default_link_args",
    actions = [
        "@rules_cc//cc/toolchains/actions:link_actions",
    ],
    args = [
        "-lrt",
        # Required for targets using std::atomic.
        "-latomic",
        "-pipe",
        # Full RELRO + BIND_NOW + non-executable stack for the IPC dispatch/plugin GOT.
        "-Wl,-z,relro,-z,now,-z,noexecstack,-z,notext",
        "-no-canonical-prefixes",
        "-pthread",
        "-Wl,--push-state",
        "-Wl,-as-needed",
        "-ldl",
        "-Wl,--pop-state",
        "-fuse-ld=gold",
        "-Wl,-no-as-needed",
        "-lc",
        "-lm",
        "-lstdc++",
        "-Wl,--build-id=sha1",
        "-Wl,--warn-execstack",
        "-Wl,--no-fatal-warnings",
    ],
    visibility = ["//quality/compiler_warnings:__subpackages__"],
)

cc_args(
    name = "treat_warnings_as_errors_args",
    actions = [
        "@rules_cc//cc/toolchains/actions:compile_actions",
    ],
    args = [
        "-Werror",
    ],
)

cc_feature(
    name = "treat_warnings_as_errors",
    args = [
        ":treat_warnings_as_errors_args",
    ],
    feature_name = "score_communication_treat_warnings_as_errors",
    visibility = ["//visibility:public"],
)

cc_args(
    name = "minimal_warnings_args",
    actions = [
        "@rules_cc//cc/toolchains/actions:compile_actions",
    ],
    args = [
        "-Wall",
        # Keep #warning visible without failing the build.
        "-Wno-error=cpp",
        "-Wno-error=deprecated-declarations",
        "-Wno-unused-macros",
        "-Wno-unused-parameter",
        "-Wno-unused-variable",
        "-Wno-format-y2k",
        "-Wno-free-nonheap-object",
        "-Wunused-but-set-parameter",
    ],
    visibility = ["//quality/compiler_warnings:__subpackages__"],
)

cc_feature(
    name = "minimal_warnings",
    args = [
        ":minimal_warnings_args",
    ],
    feature_name = "score_communication_common_minimal_warnings",
    visibility = ["//visibility:public"],
)

cc_args(
    name = "strict_warnings_args",
    actions = [
        "@rules_cc//cc/toolchains/actions:compile_actions",
    ],
    args = [
        "-Wextra",
        "-pedantic",
        "-Wconversion",  # A4-7-1,M5-0-5,M5-0-6
        "-Wsign-conversion",  # A4-7-1,M5-0-4,M5-0-9
        "-Wcast-align",
        "-Wcast-qual",
        "-Wfloat-conversion",  # M5-0-5
        "-Wformat=2",
        "-Winvalid-pch",
        "-Wmultichar",
        "-Wpacked",
        "-Wundef",
        "-Wunused-macros",
        "-Wwrite-strings",  # A2-13-4
        "-Wformat-security",
        "-Wredundant-decls",
        "-Wshadow",  # A2-10-1,A2-10-6
    ],
    visibility = ["//quality/compiler_warnings:__subpackages__"],
)

cc_feature(
    name = "strict_warnings",
    args = [
        ":strict_warnings_args",
    ],
    feature_name = "score_communication_common_strict_warnings",
    implies = [":minimal_warnings"],
    visibility = ["//visibility:public"],
)

cc_feature(
    name = "additional_warnings",
    feature_name = "score_communication_additional_warnings",
    implies = [":strict_warnings"],
    visibility = ["//visibility:public"],
)
