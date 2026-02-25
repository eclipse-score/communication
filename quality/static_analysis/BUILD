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
py_binary(
    name = "codeql_lint",
    srcs = ["codeql_lint.py"],
    args = [
        "--codeql_path=$(location @codeql_bundle//:codeql_cli)",
        "--config_path=$(location :config)",
    ],
    data = [
        ":config",
        "@codeql_bundle//:codeql_cli",
        "@codeql_coding_standards//:process_coding_standards_config",
    ],
    main = "codeql_lint.py",
    tags = ["local"],
)

filegroup(
    name = "config",
    srcs = ["config.yaml"],
    visibility = ["//visibility:public"],
)
