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
load("@review_checklists_dependencies//:requirements.bzl", "requirement")
load("@rules_python//python:defs.bzl", "py_binary", "py_library", "py_test")

py_library(
    name = "helpers",
    srcs = ["helpers.py"],
    data = ["//:review_checklists_config"],
    imports = ["."],
    visibility = ["//tools/review-checklists:__subpackages__"],
    deps = [
        requirement("bazel-runfiles"),
        requirement("gql"),
        requirement("pathspec"),
        requirement("pygithub"),
        requirement("pyyaml"),
    ],
)

py_library(
    name = "post_checklists_lib",
    srcs = ["post_checklists.py"],
    imports = ["."],
    deps = [":helpers"],
)

py_library(
    name = "check_acknowledgements_lib",
    srcs = ["check_acknowledgements.py"],
    imports = ["."],
    deps = [":helpers"],
)

py_library(
    name = "dismiss_and_invalidate_lib",
    srcs = ["dismiss_and_invalidate.py"],
    imports = ["."],
    deps = [":helpers"],
)

py_binary(
    name = "post_checklists",
    srcs = ["post_checklists.py"],
    main = "post_checklists.py",
    deps = [":helpers"],
)

py_binary(
    name = "check_acknowledgements",
    srcs = ["check_acknowledgements.py"],
    main = "check_acknowledgements.py",
    deps = [":helpers"],
)

py_binary(
    name = "dismiss_and_invalidate",
    srcs = ["dismiss_and_invalidate.py"],
    main = "dismiss_and_invalidate.py",
    deps = [":helpers"],
)

py_test(
    name = "helpers_test",
    srcs = ["helpers_test.py"],
    imports = ["."],
    deps = [
        ":helpers",
        requirement("bazel-runfiles"),
        requirement("pytest"),
    ],
)

py_test(
    name = "post_checklists_test",
    srcs = ["post_checklists_test.py"],
    imports = ["."],
    deps = [
        ":post_checklists_lib",
        requirement("bazel-runfiles"),
        requirement("pytest"),
    ],
)

py_test(
    name = "check_acknowledgements_test",
    srcs = ["check_acknowledgements_test.py"],
    imports = ["."],
    deps = [
        ":check_acknowledgements_lib",
        requirement("bazel-runfiles"),
        requirement("pytest"),
    ],
)

py_test(
    name = "dismiss_and_invalidate_test",
    srcs = ["dismiss_and_invalidate_test.py"],
    imports = ["."],
    deps = [
        ":dismiss_and_invalidate_lib",
        requirement("bazel-runfiles"),
        requirement("pytest"),
    ],
)
