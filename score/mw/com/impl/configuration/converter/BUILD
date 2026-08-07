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

load("@rules_python//python:defs.bzl", "py_binary")
load(":json_to_flatbuffer_rule.bzl", "json_to_flatbuffer")

# Exported so the configuration package's schema_drift_test can import the preprocessor.
exports_files(["json_to_flatbuffer.py"])

py_binary(
    name = "json_to_flatbuffer",
    srcs = ["json_to_flatbuffer.py"],
    main = "json_to_flatbuffer.py",
    visibility = ["//score/mw/com/impl/configuration:__subpackages__"],
)

# FlatBuffer binary of the example configuration, produced from the public JSON via the
# .fbs single source of truth. Serves as a smoke test that the schema ingests a real config.
json_to_flatbuffer(
    name = "mw_com_config",
    json = "//score/mw/com/impl/configuration:mw_com_config.json",
    visibility = ["//score/mw/com/impl/configuration:__subpackages__"],
)
