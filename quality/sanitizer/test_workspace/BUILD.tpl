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

cc_test(
    name = "asan_fail_heap_out_of_bounds",
    srcs = ["asan_fail_heap_out_of_bounds.cpp"],
)

cc_test(
    name = "tsan_fail_data_race",
    srcs = ["tsan_fail_data_race.cpp"],
)
