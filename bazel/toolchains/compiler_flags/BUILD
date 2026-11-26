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
load("@rules_cc//cc/toolchains:args.bzl", "cc_args")
load("@rules_cc//cc/toolchains:feature.bzl", "cc_feature")

cc_args(
    name = "default_compile_args",
    actions = [
        "@rules_cc//cc/toolchains/actions:compile_actions",
    ],
    args = [
        "-march=nehalem",
        "-fno-omit-frame-pointer",
        "-fPIC",
        "-fstack-protector-strong",
        "-fdiagnostics-color=always",
        # Bounds/precondition checks on libstdc++ containers (ABI-safe; not _GLIBCXX_DEBUG).
        "-D_GLIBCXX_ASSERTIONS",
        # Per-page stack probing so a large VLA/alloca in deep IPC dispatch cannot skip the guard page.
        "-fstack-clash-protection",
        # x86-64 control-flow integrity (Intel CET: indirect-branch tracking + shadow stack); pairs with -march above.
        "-fcf-protection=full",
        # Mark emitted objects' stack non-executable (complements -Wl,-z,noexecstack in the link defaults).
        "-Wa,--noexecstack",
        "-ffunction-sections",
        "-fdata-sections",
        "-no-canonical-prefixes",
        "-pthread",
    ],
    visibility = ["//bazel/toolchains/compiler_flags:__subpackages__"],
)

cc_args(
    name = "optimize_code_args",
    actions = ["@rules_cc//cc/toolchains/actions:compile_actions"],
    args = ["-O2"],
    visibility = ["//bazel/toolchains/compiler_flags:__subpackages__"],
)

cc_feature(
    name = "optimize_code",
    args = [":optimize_code_args"],
    feature_name = "score_communication_optimize_code",
    visibility = ["//visibility:public"],
)

cc_args(
    name = "fortify_source_args",
    actions = ["@rules_cc//cc/toolchains/actions:compile_actions"],
    args = [
        # Fortify requires to have an optimization level of 1 or above
        # Ideally the cc_feature should be modeled in a way that if the
        # optimization feature is not enable but you try to enable fortify
        # you get an error.
        "-U_FORTIFY_SOURCE",
        "-D_FORTIFY_SOURCE=2",
    ],
    visibility = ["//bazel/toolchains/compiler_flags:__subpackages__"],
)

cc_feature(
    name = "fortify_source",
    args = [":fortify_source_args"],
    feature_name = "score_communication_fortify_source",
    visibility = ["//visibility:public"],
)

cc_args(
    name = "default_link_args",
    actions = [
        "@rules_cc//cc/toolchains/actions:link_actions",
    ],
    args = [
        "-pipe",
        # Full RELRO + BIND_NOW + non-executable stack for the IPC dispatch/plugin GOT.
        "-Wl,-z,relro,-z,now,-z,noexecstack,-z,notext",
        "-no-canonical-prefixes",
        "-pthread",
        "-Wl,--push-state",
        "-Wl,-as-needed",
        "-ldl",
        "-Wl,--pop-state",
        "-Wl,-no-as-needed",
        "-lc",
        "-lm",
        "-lstdc++",
        "-Wl,--build-id=sha1",
        "-Wl,--warn-execstack",
        "-Wl,--fatal-warnings",
    ],
    visibility = ["//bazel/toolchains/compiler_flags:__subpackages__"],
)
