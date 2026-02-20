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

constraint_setting(
    name = "dummy_setting",
    default_constraint_value = ":always_true",
)

constraint_value(
    name = "always_true",
    constraint_setting = ":dummy_setting",
)

alias(
    name = "any_sanitizer",
    actual = select({
        "//quality/sanitizer/flags:any_sanitizer": ":always_true",
        "//conditions:default": "@platforms//:incompatible",
    }),
    visibility = ["//:__subpackages__"],
)

alias(
    name = "no_asan_ubsan_lsan",
    actual = select({
        "//quality/sanitizer/flags:asan_ubsan_lsan": "@platforms//:incompatible",
        "//conditions:default": ":always_true",
    }),
    visibility = ["//:__subpackages__"],
)

alias(
    name = "no_tsan",
    actual = select({
        "//quality/sanitizer/flags:tsan": "@platforms//:incompatible",
        "//conditions:default": ":always_true",
    }),
    visibility = ["//:__subpackages__"],
)
