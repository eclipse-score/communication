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
load("@lobster//:lobster.bzl", "lobster_trlc", "lobster_test", "lobster_gtest")
load("@rules_python//python:pip.bzl", "compile_pip_requirements")
load(":foo.bzl", "test_as_exec")
load("//source_code_linker:collect_source_files.bzl", "parse_source_files_for_needs_links")

lobster_trlc(
    name = "trlc_example",
    requirements = ["//score/mw/com/doc:comp_req_lola"],
    config = "lobster-trlc.conf",
)

test_as_exec(
    name = "foo",
    executable = "//score/mw/com:unit_test",
)

lobster_gtest(
    name = "gtest_example",
    tests =[":foo"],
    testonly = True,
)

compile_pip_requirements(
    name = "requirements",
    src = "requirements.txt.in",
    requirements_txt = "requirements.txt",
)

parse_source_files_for_needs_links(
    name = "xyz",
    srcs_and_deps = ["//score/mw/com"],
)

lobster_test(
    name = "traceability",
    inputs = [
        ":trlc_example",
        ":gtest_example",
        ":xyz",
        "@score_platform//docs/verification:system-requirements",
        "@score_platform//docs/verification:feature-requirements",
        ],

    config = "lobster.conf",
)
