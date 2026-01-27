# *******************************************************************************
# Copyright (c) 2024 Contributors to the Eclipse Foundation
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

load("@aspect_rules_lint//format:defs.bzl", "format_multirun", "format_test")
load("@rules_python//python:pip.bzl", "compile_pip_requirements")
load("@score_tooling//:defs.bzl", "copyright_checker")
load("@score_docs_as_code//:docs.bzl", "docs")

docs(source_dir = "docs")


compile_pip_requirements(
    name = "pip_requirements",
    src = "requirements.in",
    data = [
        "//quality/integration_testing:pip_requirements",
        "//third_party/traceability:pip_requirements",
    ],
    requirements_txt = "requirements_lock.txt",
)

copyright_checker(
    name = "copyright",
    srcs = [
        ".github",
        "quality",
        "score",
        "third_party",
        "//:BUILD",
        "//:MODULE.bazel",
    ],
    config = "//third_party/cr_checker:config",
    template = "//third_party/cr_checker:templates",
    visibility = ["//visibility:public"],
)

exports_files([
    ".clang-tidy",
])

format_multirun(
    name = "format",
    cc = "@clang_format//:executable",
    starlark = "@buildifier_prebuilt//:buildifier",
)

format_test(
    name = "format_test",
    cc = "@clang_format//:executable",
    no_sandbox = True,
    starlark = "@buildifier_prebuilt//:buildifier",
    tags = ["manual"],
    workspace = "//:LICENSE",
)
