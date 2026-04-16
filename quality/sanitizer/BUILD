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

filegroup(
    name = "repo_suppressions",
    srcs = [
        "asan.supp",
        "lsan.supp",
        "tsan.supp",
        "ubsan.supp",
    ],
    visibility = ["//quality/integration_testing/sanitizer_support:__pkg__"],
)

# ==============================================================================
# Canary tests — verify sanitizer instrumentation is actually active
# ==============================================================================

cc_binary(
    name = "asan_canary_bin",
    testonly = True,
    srcs = ["asan_canary.cpp"],
    target_compatible_with = ["@score_cpp_policies//sanitizers/constraints:no_tsan"],
)

sh_test(
    name = "asan_canary",
    srcs = ["verify_sanitizer_failure.sh"],
    args = [
        "$(location :asan_canary_bin)",
        "55",
    ],
    data = [":asan_canary_bin"],
    target_compatible_with = [
        "@score_cpp_policies//sanitizers/constraints:no_tsan",
        "@score_cpp_policies//sanitizers/constraints:any_sanitizer",
    ],
)

cc_binary(
    name = "tsan_canary_bin",
    testonly = True,
    srcs = ["tsan_canary.cpp"],
    target_compatible_with = ["@score_cpp_policies//sanitizers/constraints:no_asan_ubsan_lsan"],
)

sh_test(
    name = "tsan_canary",
    srcs = ["verify_sanitizer_failure.sh"],
    args = [
        "$(location :tsan_canary_bin)",
        "55",
    ],
    data = [":tsan_canary_bin"],
    target_compatible_with = [
        "@score_cpp_policies//sanitizers/constraints:no_asan_ubsan_lsan",
        "@score_cpp_policies//sanitizers/constraints:any_sanitizer",
    ],
)
