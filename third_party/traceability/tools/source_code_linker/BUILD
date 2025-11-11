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

# Python library for the source code linker package
py_library(
    name = "source_code_linker_lib",
    srcs = [
        "src/source_code_linker/__init__.py",
        "src/source_code_linker/git_utils.py",
        "src/source_code_linker/main.py",
        "src/source_code_linker/parser.py",
        "src/source_code_linker/transformer.py",
        "src/source_code_linker/writer.py",
    ],
    imports = ["src"],
    visibility = ["//visibility:public"],
)

# Entry point binary
py_binary(
    name = "parsed_source_files_for_source_code_linker",
    srcs = ["source_code_linker_main.py"],
    imports = ["."],
    main = "source_code_linker_main.py",
    visibility = ["//visibility:public"],
    deps = [":source_code_linker_lib"],
)
