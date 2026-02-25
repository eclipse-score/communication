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
filegroup(
    name = "codeql",
    srcs = glob(["**"]),
    visibility = ["//visibility:public"],
)

sh_binary(
    name = "codeql_cli",
    srcs = ["codeql/codeql"],
    data = glob(["codeql/**"]),
    visibility = ["//visibility:public"],
)
