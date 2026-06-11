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

# GCC-specific compiler warning features.
# For shared features (treat_warnings_as_errors) see //quality/compiler_warnings:BUILD.
# GCC documentation: https://gcc.gnu.org/onlinedocs/gcc/Warning-Options.html

load("@rules_cc//cc/toolchains:args.bzl", "cc_args")
load("@rules_cc//cc/toolchains:feature.bzl", "cc_feature")

# ==============================================================================
# Minimal warnings — baseline warning set for all production code.
# Enables -Wall plus GCC-specific suppressions for known false positives
# and compiler bugs.
# ==============================================================================

cc_args(
    name = "minimal_warnings_args_for_all_compile_actions",
    actions = [
        "@rules_cc//cc/toolchains/actions:compile_actions",
    ],
    args = [
        # Foundation: ~100 warnings considered essential by GCC maintainers.
        # Covers uninitialized vars, format strings, switch completeness, etc.
        # Ref: MISRA C++:2023 Dir 1.1 — compiler warnings shall be enabled.
        # Ref: https://gcc.gnu.org/onlinedocs/gcc/Warning-Options.html#index-Wall
        "-Wall",
        # Allow deprecated API usage as a warning during migration periods.
        "-Wno-error=deprecated-declarations",
        # Suppress #warning directives from failing the build.
        "-Wno-error=cpp",
        # Suppress Y2K-related strftime format warnings (irrelevant for us).
        "-Wno-format-y2k",
        # GCC false positive: warns about freeing stack memory in cases where
        # the pointer actually points to heap memory.
        "-Wno-free-nonheap-object",
        # Disabled due to false positives in GCC 8/9 with -O2 optimization.
        # See https://gcc.gnu.org/bugzilla/show_bug.cgi?id=80635
        # Real uninitialized access is still caught by -Wuninitialized (-Wall)
        # and UBSan at runtime.
        "-Wno-maybe-uninitialized",
        # Detect parameters that are assigned but never read (dead stores).
        # Ref: MISRA C++:2023 Rule 0.1.1 — unreachable code / dead stores.
        "-Wunused-but-set-parameter",
    ],
)

cc_args(
    name = "minimal_warnings_args_for_cpp_compile_actions",
    actions = [
        "@rules_cc//cc/toolchains/actions:cpp_compile_actions",
    ],
    args = [
        # GCC extension: user-defined string literal suffixes without leading
        # underscore conflict with C++11 reserved suffixes. Suppress because
        # system headers trigger this.
        "-Wno-literal-suffix",
        # GCC 7: noexcept-type mangling changed in C++17. Suppress ABI
        # compatibility warnings for pre-C++17 code.
        "-Wno-noexcept-type",
    ],
)

cc_feature(
    name = "minimal_warnings",
    args = [
        ":minimal_warnings_args_for_all_compile_actions",
        ":minimal_warnings_args_for_cpp_compile_actions",
    ],
    feature_name = "score_communication_minimal_warnings",
    visibility = ["//visibility:public"],
)

# ==============================================================================
# Strict warnings — comprehensive warning set for high-integrity code.
# Implies minimal_warnings. Separated into C, C++, and common actions
# because GCC has language-specific warnings not available across all modes.
# ==============================================================================

cc_args(
    name = "strict_warnings_args_for_all_compile_actions",
    actions = [
        "@rules_cc//cc/toolchains/actions:compile_actions",
    ],
    args = [
        # Additional warnings beyond -Wall: unused params, missing field inits,
        # implicit fallthrough, type limits, sign comparison, etc.
        # Ref: https://gcc.gnu.org/onlinedocs/gcc/Warning-Options.html#index-Wextra
        "-Wextra",
        # Enforce ISO C/C++ conformance. Diagnoses use of GNU extensions and
        # other non-standard constructs.
        # Ref: MISRA C++:2023 Rule 1.1 — code shall conform to the C++ standard.
        "-pedantic",
        # Enhanced out-of-bounds detection including pointer arithmetic results.
        # Ref: CWE-119 (Buffer Overflow), CWE-125 (Out-of-bounds Read).
        # Level 2 extends level 1 (-Wall) to intermediate pointer arithmetic.
        "-Warray-bounds=2",
        # Detect pointer casts that increase alignment requirements.
        # Misaligned access is undefined behavior on strict-alignment architectures.
        # Ref: MISRA C++:2023 Rule 8.0.1 — operations shall not lead to UB.
        # Ref: CWE-704 (Incorrect Type Conversion).
        "-Wcast-align",
        # Detect casts that remove const/volatile qualifiers.
        # Removing const and then modifying the object is undefined behavior.
        # Ref: MISRA C++:2023 Rule 8.2.3 — const qualification shall not be cast away.
        "-Wcast-qual",
        # Alert when optimization passes are disabled due to code complexity.
        # Not a correctness issue, but indicates code that may need refactoring.
        "-Wdisabled-optimization",
        # Detect implicit conversions from floating-point to integer or from
        # higher-precision to lower-precision float. Subset of -Wconversion.
        # Ref: MISRA C++:2023 Rule 7.0.5 — no implicit narrowing conversions.
        "-Wfloat-conversion",
        # Extended format string checking: includes -Wformat-nonliteral,
        # -Wformat-security, and -Wformat-y2k on top of basic -Wformat.
        # Ref: CWE-134 (Uncontrolled Format String).
        # Ref: CERT C FIO30-C — exclude user input from format strings.
        "-Wformat=2",
        # Require explicit fallthrough annotation in switch cases (level 4:
        # only recognizes [[fallthrough]], __attribute__((fallthrough)), and
        # uppercase FALLTHRU comment).
        # Ref: MISRA C++:2023 Rule 9.6.5 — switch cases shall not fall through.
        "-Wimplicit-fallthrough=4",
        # Warn if a precompiled header cannot be used. Build hygiene.
        "-Winvalid-pch",
        # Suggest adding format attributes to functions that call printf/scanf.
        # Improves format string type safety across the codebase.
        "-Wmissing-format-attribute",
        # Warn on multi-character constants ('ABCD'). These have
        # implementation-defined values and are not portable.
        # Ref: MISRA C++:2023 Rule 5.13.2 — use of implementation-defined literals.
        "-Wmultichar",
        # Warn if __attribute__((packed)) has no effect on struct layout.
        # Indicates a misunderstanding of packing semantics.
        "-Wpacked",
        # Warn about scalar storage order (endianness) issues.
        # Relevant for cross-platform code handling binary data.
        "-Wscalar-storage-order",
        # Suggest adding __attribute__((format)) to functions wrapping
        # printf/scanf, improving format string validation.
        "-Wsuggest-attribute=format",
        # Warn if an undefined identifier is used in #if directives.
        # Undefined macros silently evaluate to 0, hiding configuration errors.
        # Ref: MISRA C:2012 Rule 20.9 / MISRA C++:2023 Dir 4.4.
        "-Wundef",
        # Warn about macros defined but never expanded or tested.
        # Indicates dead code or incomplete refactoring.
        "-Wunused-macros",
        # Warn when vector operations are not implemented via SIMD but are
        # emulated piecewise. Performance tuning aid.
        "-Wvector-operation-performance",
        # In C, make string literals const char[] so that modifying them
        # produces a warning. String literal modification is undefined behavior.
        # Ref: MISRA C++:2023 Rule 5.13.5 — string literals shall not be modified.
        # Ref: CWE-704 (Incorrect Type Conversion).
        "-Wwrite-strings",
        # Detect format string vulnerabilities (printf with non-literal format).
        # Ref: CWE-134 (Uncontrolled Format String).
        "-Wformat-security",
        # Detect suspicious uses of logical operators (e.g., duplicate operands,
        # bitwise where logical is expected).
        # Ref: MISRA C++:2023 Rule 8.9.1 — logical operators shall not be misused.
        "-Wlogical-op",
        # Detect duplicate declarations in the same scope.
        # Indicates copy-paste errors or incomplete refactoring.
        "-Wredundant-decls",
        # Detect variable shadowing: inner scope variables hiding outer ones.
        # Ref: MISRA C++:2023 Rule 6.4.1 — identifiers shall not be hidden.
        "-Wshadow",
        # Detect implicit narrowing conversions (int64 -> int32, double -> float).
        # Ref: MISRA C++:2023 Rule 7.0.5 — no implicit narrowing conversions.
        # Ref: CWE-681 (Incorrect Conversion between Numeric Types).
        "-Wconversion",
        # Detect implicit sign changes (signed <-> unsigned).
        # Ref: MISRA C++:2023 Rule 7.0.5/7.0.6 — sign conversion awareness.
        # Ref: CWE-195 (Signed to Unsigned Conversion Error).
        "-Wsign-conversion",
    ],
)

cc_args(
    name = "strict_warnings_args_for_cpp_compile_actions",
    actions = [
        "@rules_cc//cc/toolchains/actions:cpp_compile_actions",
    ],
    args = [
        # Warn when deleting an object through a pointer to a base class that
        # has virtual functions but no virtual destructor. This is UB.
        # Ref: MISRA C++:2023 Rule 15.7.1 — destructor of a polymorphic base
        # class shall be virtual.
        # Ref: CWE-weaknesses related to improper resource cleanup.
        "-Wdelete-non-virtual-dtor",
        # Detect when a derived class function hides (rather than overrides) a
        # base class virtual function due to different signature.
        # Ref: MISRA C++:2023 Rule 10.2.0 — overriding virtual functions.
        "-Woverloaded-virtual",
        # Warn on use of the 'register' storage class specifier, which is
        # deprecated in C++11 and removed in C++17.
        "-Wregister",
        # GCC-specific: warn when a variadic function call is missing the
        # NULL sentinel value at the end of the argument list.
        "-Wstrict-null-sentinel",
    ],
)

cc_args(
    name = "strict_warnings_args_for_c_compile_actions",
    actions = [
        "@rules_cc//cc/toolchains/actions:c_compile_actions",
    ],
    args = [
        # Warn about K&R-style (old-style) function definitions.
        # Ref: MISRA C:2012 Rule 8.2 — function types shall be in prototype form.
        "-Wold-style-definition",
        # Enforce function prototypes. Functions declared without parameter
        # types provide no type checking at call sites.
        # Ref: MISRA C:2012 Rule 8.2 — function types shall be in prototype form.
        "-Wstrict-prototypes",
    ],
)

cc_feature(
    name = "strict_warnings",
    args = [
        ":strict_warnings_args_for_all_compile_actions",
        ":strict_warnings_args_for_cpp_compile_actions",
        ":strict_warnings_args_for_c_compile_actions",
    ],
    feature_name = "score_communication_strict_warnings",
    implies = ["minimal_warnings"],
    visibility = ["//visibility:public"],
)

# ==============================================================================
# Third-party warnings — suppress ALL warnings for vendored/external code.
# Mutually exclusive with treat_warnings_as_errors and minimal_warnings.
# ==============================================================================

cc_args(
    name = "third_party_warnings_args",
    actions = [
        "@rules_cc//cc/toolchains/actions:compile_actions",
    ],
    args = [
        # Suppress all warnings. Used for third-party code where we cannot
        # fix upstream warnings and they would only add noise.
        # GCC uses -w (short form). Equivalent to Clang's --no-warnings.
        "-w",
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
