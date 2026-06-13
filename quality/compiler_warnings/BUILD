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

# Common compiler warning features shared across ALL compilers (GCC, Clang).
# Compiler-specific features live in clang/BUILD and gcc/BUILD respectively.

load("@rules_cc//cc/toolchains:args.bzl", "cc_args")
load("@rules_cc//cc/toolchains:feature.bzl", "cc_feature")

# ==============================================================================
# Default compile/link flags (LLVM/Clang toolchain baseline)
# ==============================================================================

cc_args(
    name = "default_compile_args",
    actions = [
        "@rules_cc//cc/toolchains/actions:compile_actions",
    ],
    args = [
        "-march=nehalem",
        # IEEE 754 strict floating-point model — prevents unsafe FP optimizations.
        # Mitigates MISRA C++:2023 Rule 0.4.2 (floating-point arithmetic precision).
        "-ffp-model=strict",
        # Security hardening flags
        # Undefine _FORTIFY_SOURCE to avoid conflict with sanitizers.
        # See https://github.com/google/sanitizers/issues/247
        "-U_FORTIFY_SOURCE",
        # Stack buffer overflow protection (CWE-121).
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

# ==============================================================================
# Treat warnings as errors — shared across all compilers
# MISRA C++:2023 Dir 1.1 — All compiler warnings relevant to the project
# shall be enabled and treated as errors.
# ==============================================================================

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

# ==============================================================================
# Additional warnings — extra diagnostics beyond strict_warnings.
# These flags have high value but may produce more noise on legacy code.
# Shared across GCC and Clang (both support all flags listed here).
# ==============================================================================

cc_args(
    name = "additional_warnings_args",
    actions = [
        "@rules_cc//cc/toolchains/actions:compile_actions",
    ],
    args = [
        # Detect pointer casts that increase alignment requirements.
        # Misaligned access is UB on strict-alignment architectures.
        # Ref: MISRA C++:2023 Rule 8.0.1 — operations shall not lead to UB.
        # Ref: CWE-704 (Incorrect Type Conversion).
        "-Wcast-align",
        # Detect casts that remove const/volatile qualifiers.
        # Removing const then modifying the object is undefined behavior.
        # Ref: MISRA C++:2023 Rule 8.2.3 — const shall not be cast away.
        "-Wcast-qual",
        # Warn when float is implicitly promoted to double on 32-bit FPU targets.
        # On such targets this causes expensive software emulation.
        # Ref: MISRA C++:2023 Rule 0.4.2 (precision awareness).
        "-Wdouble-promotion",
        # Detect non-literal format strings passed to printf/scanf family.
        # Non-literal format strings cannot be statically validated.
        # Ref: CWE-134 (Uncontrolled Format String).
        # Ref: CERT C FIO30-C.
        "-Wformat-nonliteral",
        # Warn about anything depending on "size of" void or function types.
        # GNU C assigns these size 1, but this is non-portable.
        # Ref: MISRA C++:2023 Rule 1.1 — conformance to the standard.
        "-Wpointer-arith",
        # Detect duplicate declarations in the same scope.
        # Indicates copy-paste errors or incomplete refactoring.
        "-Wredundant-decls",
        # Warn if undefined identifier used in #if directives.
        # Undefined macros silently evaluate to 0, hiding config errors.
        # Ref: MISRA C:2012 Rule 20.9 / MISRA C++:2023 Dir 4.4.
        "-Wundef",
        # Make string literals const char[] in C. Modification is UB.
        # Ref: MISRA C++:2023 Rule 5.13.5 — string literals not modified.
        "-Wwrite-strings",
    ],
)

cc_feature(
    name = "additional_warnings",
    args = [
        ":additional_warnings_args",
    ],
    feature_name = "score_communication_additional_warnings",
    visibility = ["//visibility:public"],
)
