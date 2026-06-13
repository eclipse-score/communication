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

# Clang/LLVM-specific compiler warning features.
# Used with toolchains_llvm via extra_known_features in MODULE.bazel.
# For shared features see //quality/compiler_warnings:BUILD.

load("@rules_cc//cc/toolchains:args.bzl", "cc_args")
load("@rules_cc//cc/toolchains:feature.bzl", "cc_feature")

# ==============================================================================
# Minimal warnings — baseline warning set for all production code.
# Enables -Wall plus suppressions for known false positives in Clang.
# ==============================================================================

cc_args(
    name = "minimal_warnings_args",
    actions = [
        "@rules_cc//cc/toolchains/actions:compile_actions",
    ],
    args = [
        # Foundation: enables ~100 warnings considered essential by compiler
        # maintainers. Covers uninitialized variables, format strings, switch
        # completeness, dangling pointers, return types, and more.
        # Ref: MISRA C++:2023 Dir 1.1 — compiler warnings shall be enabled.
        # Ref: https://clang.llvm.org/docs/DiagnosticsReference.html#wall
        "-Wall",
        # Suppress #warning directives from failing the build.
        # #warning is used intentionally in some headers for deprecation notices.
        "-Wno-error=cpp",
        # Allow deprecated API usage as a warning, not an error.
        # Needed during gradual API migration periods.
        "-Wno-error=deprecated-declarations",
        # Suppress false positive from Clang's self-assignment overloaded check.
        # See https://bugs.llvm.org/show_bug.cgi?id=43124
        "-Wno-error=self-assign-overloaded",
        # Clang Thread Safety Analysis. Detects potential data races through
        # annotations (GUARDED_BY, REQUIRES, etc.). Essential for concurrent code.
        # Ref: CERT C++ CON43-CPP — do not allow data races.
        # Clang-specific: not available in GCC.
        # Ref: https://clang.llvm.org/docs/ThreadSafetyAnalysis.html
        "-Wthread-safety",
    ],
)

cc_feature(
    name = "minimal_warnings",
    args = [
        ":minimal_warnings_args",
    ],
    feature_name = "score_communication_minimal_warnings",
    visibility = ["//visibility:public"],
)

# ==============================================================================
# Strict warnings — comprehensive warning set for high-integrity code.
# Implies minimal_warnings. Only flags with high signal-to-noise ratio
# that reliably detect real bugs are included here.
# ==============================================================================

cc_args(
    name = "strict_warnings_args",
    actions = [
        "@rules_cc//cc/toolchains/actions:compile_actions",
    ],
    args = [
        # Additional warnings not in -Wall. Covers unused parameters, missing
        # field initializers, sign comparison, type limits, and more.
        # Ref: https://clang.llvm.org/docs/DiagnosticsReference.html#wextra
        "-Wextra",
        # Enforce ISO C++ conformance. Diagnoses non-standard extensions.
        # Ref: MISRA C++:2023 Rule 1.1 — code shall conform to the C++ standard.
        "-pedantic",
        # Detect implicit conversions that may lose data (e.g., int64_t to int32_t,
        # double to float). Catches truncation and precision-loss bugs.
        # Ref: MISRA C++:2023 Rule 7.0.5 — no implicit narrowing conversions.
        # Ref: CWE-681 (Incorrect Conversion between Numeric Types).
        "-Wconversion",
        # Warn on direct floating-point equality comparisons (==, !=).
        # Floating-point arithmetic is inherently imprecise; equality checks are
        # almost always a bug. Use epsilon-based comparison instead.
        # Ref: MISRA C:2012 Rule 13.6 / MISRA C++:2023 Rule 0.4.2.
        "-Wfloat-equal",
        # Detect format string vulnerabilities: printf/scanf calls where the
        # format string is not a literal and could be attacker-controlled.
        # Ref: CWE-134 (Use of Externally-Controlled Format String).
        # Ref: CERT C FIO30-C — exclude user input from format strings.
        "-Wformat-security",
        # Detect implicit sign conversions (signed <-> unsigned). These can
        # silently change values (e.g., -1 becoming a huge positive number).
        # Ref: MISRA C++:2023 Rule 7.0.5/7.0.6 — sign conversion awareness.
        # Ref: CWE-195 (Signed to Unsigned Conversion Error).
        "-Wsign-conversion",
        # Comprehensive variable shadowing detection. Includes shadowing of
        # member variables, not just local-to-local shadowing (-Wshadow).
        # Ref: MISRA C++:2023 Rule 6.4.1 — identifiers shall not be hidden.
        # Clang-specific: -Wshadow-all is a superset of GCC's -Wshadow.
        "-Wshadow-all",
        # -Wshadow-all sub-categories are NOT subcategories of -Wshadow for
        # -Wno-error purposes. Downgrade these individually so that the global
        # -Werror does not reject them during migration.
        "-Wno-error=shadow-uncaptured-local",
        "-Wno-error=shadow-field",
    ],
)

cc_feature(
    name = "strict_warnings",
    args = [
        ":strict_warnings_args",
    ],
    feature_name = "score_communication_strict_warnings",
    implies = ["minimal_warnings"],
    visibility = ["//visibility:public"],
)

# ==============================================================================
# Third-party warnings — suppress ALL warnings for vendored/external code.
# Mutually exclusive with treat_warnings_as_errors and minimal_warnings
# to avoid contradictory flags.
# ==============================================================================

cc_args(
    name = "third_party_warnings_args",
    actions = [
        "@rules_cc//cc/toolchains/actions:compile_actions",
    ],
    args = [
        # Suppress all warnings. Used for third-party code where we cannot
        # fix upstream warnings and they would only add noise.
        "--no-warnings",
    ],
)

cc_feature(
    name = "third_party_warnings",
    args = [
        ":third_party_warnings_args",
    ],
    feature_name = "score_communication_third_party_warnings",
    mutually_exclusive = [
        "//quality/compiler_warnings:treat_warnings_as_errors",
        ":minimal_warnings",
    ],
    visibility = ["//visibility:public"],
)
