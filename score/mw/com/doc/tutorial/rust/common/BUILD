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

load("@rules_rust//rust:defs.bzl", "rust_library")

# Runtime setup, logging and type aliases shared by all Rust tutorial chapters.
# The generated Vehicle interface lives in //score/mw/com/doc/tutorial/rust/common/com-api-gen.
rust_library(
    name = "common",
    srcs = [
        "lib.rs",
        "mixed_monitor.rs",
    ],
    crate_name = "tutorial_common",
    features = ["link_std_cpp_lib"],
    visibility = ["//score/mw/com/doc:__subpackages__"],
    deps = [
        "//score/mw/com/doc/tutorial/rust/common/com-api-gen",
        "//score/mw/com/rust:score_com",
        "@score_baselibs//score/log_rust/stdout_logger",
    ],
)

# Configuration files used at runtime by the sample applications.
filegroup(
    name = "config",
    srcs = [
        "etc/logging.json",
        "etc/mw_com_config.json",
    ],
    visibility = ["//score/mw/com/doc:__subpackages__"],
)

# Sources referenced via literalinclude from the tutorial chapters.
filegroup(
    name = "doc_files",
    srcs = [
        "etc/mw_com_config.json",
        "lib.rs",
        "mixed_monitor.rs",
        "//score/mw/com/doc/tutorial/rust/common/com-api-gen:doc_files",
    ],
    visibility = ["//score/mw/com/doc/tutorial/rust:__pkg__"],
)
