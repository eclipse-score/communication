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

# This is not a `py_library`, since we need it as "main" in a `py_test`
exports_files(
    ["main.py", "pytest.toml"],
)

filegroup(
    name = "pip_requirements",
    srcs = ["requirements.txt"],
    visibility = ["//:__pkg__"],
)

py_library(
    name = "system_under_test",
    srcs = ["system_under_test.py"],
    target_compatible_with = ["@platforms//os:linux"],
    visibility = ["//quality/integration_testing/environments:__subpackages__"],
)
