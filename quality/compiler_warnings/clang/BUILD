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

cc_args(
    name = "default_compile_args",
    actions = [
        "@rules_cc//cc/toolchains/actions:compile_actions",
    ],
    args = [
        "-ffp-model=strict",
        "-fbracket-depth=512",
    ],
)

# The sysroot needs its "mergedusr" bin/lib/lib64 compat symlinks (see
# MODULE.bazel's ubuntu24_04_sysroot apt.install()) so that glibc's linker-script
# stub .so files (e.g. libm.so's `GROUP ( /lib/x86_64-linux-gnu/libm.so.6 ... )`,
# which hardcodes an absolute, non-merged-usr path) resolve correctly at link time.
# But those same symlinks make Clang's GCC-installation auto-detection pick an
# ambiguous, symlink-relative candidate (<sysroot>/lib/gcc/<triple>/13) instead of
# the canonical one (<sysroot>/usr/lib/gcc/<triple>/13); the resulting relative-path
# math to libstdc++'s headers only resolves if the execution environment
# transparently follows the symlink during path traversal, which does not happen
# the same way on RBE workers and causes "cstddef file not found" errors. Pin the
# canonical, non-symlinked GCC install directory explicitly so Clang's detector
# never has to guess.
cc_args(
    name = "gcc_install_dir_args_amd64",
    actions = [
        "@rules_cc//cc/toolchains/actions:compile_actions",
    ],
    args = ["--gcc-install-dir={sysroot}/usr/lib/gcc/x86_64-linux-gnu/13"],
    data = ["@ubuntu24_04_sysroot_amd64//sysroot"],
    format = {"sysroot": "@ubuntu24_04_sysroot_amd64//sysroot"},
)

cc_args(
    name = "gcc_install_dir_args_arm64",
    actions = [
        "@rules_cc//cc/toolchains/actions:compile_actions",
    ],
    args = ["--gcc-install-dir={sysroot}/usr/lib/gcc/aarch64-linux-gnu/13"],
    data = ["@ubuntu24_04_sysroot_arm64//sysroot"],
    format = {"sysroot": "@ubuntu24_04_sysroot_arm64//sysroot"},
)

# Complete default flag set for the LLVM/Clang toolchain: the shared compile and
# link defaults plus the Clang-only compile args, combined into a single feature.
cc_feature(
    name = "default_flags",
    args = [
        "//quality/compiler_warnings:default_compile_args",
        "//quality/compiler_warnings:default_link_args",
        ":default_compile_args",
    ] + select(
        {
            "@platforms//cpu:aarch64": [":gcc_install_dir_args_arm64"],
            "@platforms//cpu:x86_64": [":gcc_install_dir_args_amd64"],
        },
        no_match_error = "Unsupported platform",
    ),
    feature_name = "score_communication_clang_default_flags",
    visibility = ["//visibility:public"],
)

cc_args(
    name = "minimal_warnings_args",
    actions = [
        "@rules_cc//cc/toolchains/actions:compile_actions",
    ],
    args = [
        # Suppress false positive from Clang's self-assignment overloaded check.
        # See https://bugs.llvm.org/show_bug.cgi?id=43124
        "-Wno-error=self-assign-overloaded",
        "-Wno-return-type-c-linkage",
        "-Wno-unused-command-line-argument",
        # Clang-only: GCC has no -Wdeprecated-non-prototype (C2x non-prototype decls).
        "-Wno-deprecated-non-prototype",
    ],
)

cc_feature(
    name = "minimal_warnings",
    args = [
        ":minimal_warnings_args",
    ],
    feature_name = "score_communication_minimal_warnings",
    implies = ["//quality/compiler_warnings:minimal_warnings"],
    visibility = ["//visibility:public"],
)

cc_args(
    name = "strict_warnings_args",
    actions = [
        "@rules_cc//cc/toolchains/actions:compile_actions",
    ],
    args = [
        "-Warray-bounds",
        "-Wimplicit-fallthrough",
    ],
)

cc_feature(
    name = "strict_warnings",
    args = [
        ":strict_warnings_args",
    ],
    feature_name = "score_communication_strict_warnings",
    implies = [
        "//quality/compiler_warnings:strict_warnings",
        "minimal_warnings",
    ],
    visibility = ["//visibility:public"],
)

cc_args(
    name = "strict_warnings_no_error_args",
    actions = [
        "@rules_cc//cc/toolchains/actions:compile_actions",
    ],
    args = [
        "-Wno-error=shadow",
        "-Wno-error=conversion",
        "-Wno-error=sign-conversion",
    ],
)

cc_feature(
    name = "strict_warnings_no_error",
    # Emit -Werror first, then the -Wno-error= demotions, so the demotions
    # reliably take effect (ordering matters: a trailing blanket -Werror would
    # re-promote them to errors).
    args = [
        "//quality/compiler_warnings:treat_warnings_as_errors_args",
        ":strict_warnings_no_error_args",
    ],
    feature_name = "score_communication_strict_warnings_no_error",
    implies = ["strict_warnings"],
    visibility = ["//visibility:public"],
)

cc_feature(
    name = "additional_warnings",
    args = [
        "//quality/compiler_warnings:additional_warnings_args",
    ],
    feature_name = "score_communication_additional_warnings",
    implies = [":strict_warnings"],
    visibility = ["//visibility:public"],
)

cc_args(
    name = "third_party_warnings_args",
    actions = [
        "@rules_cc//cc/toolchains/actions:compile_actions",
    ],
    args = ["-w"],
)

cc_feature(
    name = "third_party_warnings",
    args = [
        ":third_party_warnings_args",
    ],
    feature_name = "score_communication_third_party_warnings",
    mutually_exclusive = [
        ":additional_warnings",
        "//quality/compiler_warnings:treat_warnings_as_errors",
        ":minimal_warnings",
        ":strict_warnings",
    ],
    visibility = ["//visibility:public"],
)
