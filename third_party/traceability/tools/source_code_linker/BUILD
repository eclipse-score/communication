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

# Carry over from Eclipse S-Core including slight modifications:
# https://github.com/eclipse-score/docs-as-code/tree/v0.4.0/src/extensions/score_source_code_linker

py_binary(
    name = "parsed_source_files_for_source_code_linker",
    srcs = [
        "get_git_info.py",
        "parse_source_files.py",
    ],
    main = "parse_source_files.py",
    imports = ["."],
    visibility = ["//visibility:public"],
)
