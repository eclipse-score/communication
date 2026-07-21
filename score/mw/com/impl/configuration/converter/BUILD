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

load("@rules_python//python:defs.bzl", "py_binary", "py_test")
load("//score/mw/com/impl/configuration:flatbuffers_flags.bzl", "FLATBUFFERS_TARGET_COMPATIBLE_WITH")
load(":json_to_flatbuffer_rule.bzl", "mw_com_config_to_flatbuffer")

py_binary(
    name = "json_to_flatbuffer",
    srcs = [
        "json_to_flatbuffer.py",
    ],
    imports = ["."],
    main = "json_to_flatbuffer.py",
    visibility = ["//visibility:public"],
)

# Requires FlatBuffers support (see //score/mw/com/impl/configuration:flatbuffers_flags.bzl):
#   bazel test --config=flatbuffers :json_to_flatbuffer_test
py_test(
    name = "json_to_flatbuffer_test",
    size = "small",
    srcs = [
        "json_to_flatbuffer.py",
        "json_to_flatbuffer_test.py",
    ],
    data = [
        "//score/mw/com/impl/configuration:example/mw_com_config.json",
        "//score/mw/com/impl/configuration:mw_com_config.fbs",
        "@flatbuffers//:flatc",
    ],
    env = {
        "FLATC_PATH": "$(rootpath @flatbuffers//:flatc)",
    },
    imports = ["."],
    main = "json_to_flatbuffer_test.py",
    target_compatible_with = FLATBUFFERS_TARGET_COMPATIBLE_WITH,
)

# Example: build the FlatBuffer binary for the example configuration as part of
# the build graph. Requires FlatBuffers support to be enabled:
#   bazel build --config=flatbuffers //score/mw/com/impl/configuration/converter:mw_com_config.bin
mw_com_config_to_flatbuffer(
    name = "mw_com_config.bin",
    out = "mw_com_config.bin",
    config_json = "//score/mw/com/impl/configuration:example/mw_com_config.json",
    target_compatible_with = FLATBUFFERS_TARGET_COMPATIBLE_WITH,
    visibility = ["//visibility:public"],
)
