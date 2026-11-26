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
    name = "treat_warnings_as_errors_args",
    actions = [
        "@rules_cc//cc/toolchains/actions:compile_actions",
    ],
    args = [
        "-Werror",
    ],
    visibility = ["//quality/compiler_warnings:__subpackages__"],
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
        "-Wunused-but-set-parameter",
        "-Wextra",
        "-pedantic",
        "-Wconversion",  # A4-7-1,M5-0-5,M5-0-6
        "-Wsign-conversion",  # A4-7-1,M5-0-4,M5-0-9
        "-Wfloat-conversion",  # M5-0-5
        "-Wfloat-equal",
        "-Wformat=2",
        "-Winvalid-pch",
        "-Wmultichar",
        "-Wpacked",
        "-Wunused-macros",
        "-Wformat-security",
        "-Wshadow",  # A2-10-1,A2-10-6
        "-Wcast-align",
        "-Wcast-qual",
        "-Wundef",
        "-Wwrite-strings",  # A2-13-4
        "-Wredundant-decls",
    ],
    visibility = ["//quality/compiler_warnings:__subpackages__"],
)

cc_args(
    name = "misra_warnings_args",
    actions = [
        "@rules_cc//cc/toolchains/actions:cpp_compile_actions",
    ],
    # This contains the list of warnings that we decided to enable for C++ to help fulfilling
    # MISRA C++:2023. This list contains only the warnings that are common to gcc and clang.
    # The warnings that are specific to gcc or clang can be found in the respective folders.
    args = [
        "-Wtype-limits",  # Helps to achieve MISRA C++:2023 Rule 0-0-2
        "-Wunused-variable",  # Helps to achieve MISRA C++:2023 Rule 0-2-1 in gcc and clang and Rule 0-1-1 in clang
        "-Wunused-parameter",  # Helps to achieve MISRA C++:2023 Rule 0-2-2
        "-Wunused-function",  # Helps to achieve MISRA C++:2023 Rule 0-2-4
        "-Wtrigraphs",  # Helps to achieve MISRA C++:2023 Rule 5-0-1
        "-Wcomment",  # Helps to achieve MISRA C++:2023 Rule 5-7-1 and 5-7-3
        "-Wshadow",  # Helps to achieve MISRA C++:2023 Rule 6-4-1 in gcc and clang and Rule 10-2-2 in clang
        "-Wparentheses",  # Helps to achieve MISRA C++:2023 Rule 8-0-1, 8-18-2, and 9-3-1 in gcc and clang and Rule 6-0-1 in gcc
        "-Wswitch-bool",  # Helps to achieve MISRA C++:2023 Rule 7-0-1
        "-Wsign-compare",  # Helps to achieve MISRA C++:2023 Rule 7-0-5
        "-Wconversion",  # Helps to achieve MISRA C++:2023 Rule 7-0-6 in gcc and clang, Rule 7-0-5 in gcc, and Rule 7-0-3 and 8-20-1 in clang
        "-Waddress",  # Helps to achieve MISRA C++:2023 Rule 7-11-3
        "-Wdangling-else",  # Helps to achieve MISRA C++:2023 Rule 9-3-1
        "-Wmisleading-indentation",  # Helps to achieve MISRA C++:2023 Rule 9-3-1
        "-Wswitch-enum",  # Helps to achieve MISRA C++:2023 Rule 9-4-2
        "-Wreturn-type",  # Helps to achieve MISRA C++:2023 Rule 9-6-5
        "-Wignored-qualifiers",  # Helps to achieve MISRA C++:2023 Rule 10-1-2
        "-Wenum-compare",  # Helps to achieve MISRA C++:2023 Rule 10-2-3
        "-Wuninitialized",  # Helps to achieve MISRA C++:2023 Rule 11-6-2
        "-Wexpansion-to-defined",  # Helps to achieve MISRA C++:2023 Rule 19-1-1
        "-Wdelete-incomplete",  # Helps to achieve MISRA C++:2023 Rule 21-6-5
    ],
    visibility = ["//quality/compiler_warnings:__subpackages__"],
)
