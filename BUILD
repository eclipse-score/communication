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
load("@rules_rust//rust:defs.bzl", "rust_clippy")
load("@score_tooling//:defs.bzl", "copyright_checker")

compile_pip_requirements(
    name = "pip_requirements",
    src = "requirements.in",
    data = ["//quality/integration_testing:pip_requirements"],
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
    "wait_free_stack_fix.patch",
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

rust_clippy(
    name = "clippy",
    testonly = True,
    tags = ["manual"],
    visibility = ["//:__subpackages__"],
    deps = [
        "//score/mw/com/example/com-api-example/com-api-gen:com-api-gen",
        "//score/mw/com/example/com-api-example:com-api-example",
        "//score/mw/com/example/ipc_bridge:ipc_bridge_rs",
        "//score/mw/com/example/ipc_bridge:ipc_bridge_gen_rs",
        "//score/mw/com/example/com-api-example:com-api-example-test",
        "//score/mw/com/impl/plumbing:sample_ptr_rs",
        "//score/mw/com/impl/rust:common",
        "//score/mw/com/impl/rust:macros",
        "//score/mw/com/impl/rust:mw_com",
        "//score/mw/com/impl/rust:proxy_bridge_rs",
        "//score/mw/com/impl/rust:skeleton_bridge_rs",
        "//score/mw/com/impl/rust/com-api/com-api:com-api",
        "//score/mw/com/impl/rust/com-api/com-api-concept:com-api-concept",
        "//score/mw/com/impl/rust/com-api/com-api-concept:com-api-concept-test",
        "//score/mw/com/impl/rust/com-api/com-api-runtime-lola:com-api-runtime-lola",
        "//score/mw/com/impl/rust/com-api/com-api-runtime-mock:com-api-runtime-mock",
        "//score/mw/com/impl/rust/test:proxy_bridge_integration_test",
    ],
)
