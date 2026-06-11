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
# Compiler-specific warning sets are defined in clang/BUILD and gcc/BUILD.

load("@rules_cc//cc/toolchains:args.bzl", "cc_args")
load("@rules_cc//cc/toolchains:feature.bzl", "cc_feature")

# Default compile and link flags used across the project.

cc_args(
    name = "default_compile_args",
    actions = [
        "@rules_cc//cc/toolchains/actions:compile_actions",
    ],
    args = [
        "-march=nehalem",
        # Keep floating-point behavior predictable across optimizations.
        "-ffp-model=strict",
        # Undefine _FORTIFY_SOURCE to avoid conflict with sanitizers.
        # See https://github.com/google/sanitizers/issues/247
        "-U_FORTIFY_SOURCE",
        # Enable stack canaries.
        "-fstack-protector",
        # Preserve frame pointers for reliable stack traces in crash diagnostics.
        "-fno-omit-frame-pointer",
        # Colored compiler output for better developer experience.
        "-fcolor-diagnostics",
    ],
)

cc_feature(
    name = "default_flags",
    args = [
        ":default_compile_args",
    ],
    feature_name = "score_communication_default_flags",
    visibility = ["//visibility:public"],
)

# Default link flags — shared across all compilers.

cc_args(
    name = "default_link_args",
    actions = [
        "@rules_cc//cc/toolchains/actions:link_actions",
    ],
    args = [
        "-lrt",
        # This is the agreed way to ensure linking for targets using
        # std::atomic operations.
        "-latomic",
    ],
)

cc_feature(
    name = "default_link_flags",
    args = [
        ":default_link_args",
    ],
    feature_name = "score_communication_default_link_flags",
    visibility = ["//visibility:public"],
)

# Treat warnings as errors for first-party code.

cc_args(
    name = "treat_warnings_as_errors_args",
    actions = [
        "@rules_cc//cc/toolchains/actions:compile_actions",
    ],
    args = [
        # Promotes all warnings to errors so that no warning is silently ignored.
        # This is the core mechanism for enforcing warning-free builds.
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
        # Essential diagnostics
        "-Wall",
        # Suppress #warning directives from failing the build.
        # #warning is used intentionally in some headers for deprecation notices.
        "-Wno-error=cpp",
        # Allow deprecated API usage as a warning, not an error.
        # Needed during gradual API migration periods.
        "-Wno-error=deprecated-declarations",
    ],
    visibility = ["//quality/compiler_warnings:__subpackages__"],
)

cc_args(
    name = "strict_warnings_args",
    actions = [
        "@rules_cc//cc/toolchains/actions:compile_actions",
    ],
    args = [
        "-Wextra",
        "-Wpedantic",
        "-Wconversion",
        "-Wsign-conversion",
    ],
    visibility = ["//quality/compiler_warnings:__subpackages__"],
)

# Additional diagnostics shared across GCC and Clang.

cc_args(
    name = "additional_warnings_args",
    actions = [
        "@rules_cc//cc/toolchains/actions:compile_actions",
    ],
    args = [
        # Detect pointer casts that increase alignment requirements.
        # Misaligned access is UB on strict-alignment architectures.
        "-Wcast-align",
        # Detect casts that remove const/volatile qualifiers.
        # Removing const then modifying the object is undefined behavior.
        "-Wcast-qual",
        # Warn when float is implicitly promoted to double on 32-bit FPU targets.
        # On such targets this causes expensive software emulation.
        "-Wdouble-promotion",
        # Detect non-literal format strings passed to printf/scanf family.
        # Non-literal format strings cannot be statically validated.
        "-Wformat-nonliteral",
        # Warn about anything depending on "size of" void or function types.
        # GNU C assigns these size 1, but this is non-portable.
        "-Wpointer-arith",
        # Detect duplicate declarations in the same scope.
        # Indicates copy-paste errors or incomplete refactoring.
        "-Wredundant-decls",
        # Warn if undefined identifier used in #if directives.
        # Undefined macros silently evaluate to 0, hiding config errors.
        "-Wundef",
        # Make string literals const char[] in C. Modification is UB.
        "-Wwrite-strings",
    ],
    visibility = ["//quality/compiler_warnings:__subpackages__"],
)

cc_feature(
    name = "additional_warnings",
    args = [
        ":additional_warnings_args",
    ],
    feature_name = "score_communication_additional_warnings",
    visibility = ["//visibility:public"],
)
