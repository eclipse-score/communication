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

sh_test(
    name = "sanitizers_functional_test",
    srcs = ["sanitizers_functional_test.sh"],
    data = [
        "sanitizer.bazelrc",
        "test_workspace/BUILD.tpl",
        "test_workspace/MODULE.bazel",
        "test_workspace/asan_fail_heap_out_of_bounds.cpp",
        "test_workspace/lsan_fail_leak.cpp",
        "test_workspace/tsan_fail_data_race.cpp",
    ],
    env_inherit = [
        "PATH",
        "http_proxy",
        "https_proxy",
    ],
    tags = [
        "local",  # Test marked local because it uses bazel.
        "manual",  # Due to long-runtime and local dependencies
    ],
    deps = ["@bazel_tools//tools/bash/runfiles"],
)
