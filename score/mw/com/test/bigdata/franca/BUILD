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

load("//ecu/xpad/ara/bmw/tools:ara_arxml_inject_non_reporting_behavior.bzl", "ara_arxml_inject_non_reporting_behavior")
load("//ecu/xpad/ara/vector/tools/amsr_generators:amsrgen_tree.bzl", "ara_vector_artifacts", "filegroup_ara_vector_configs")
load("//score/bazel/app_config:app_config.bzl", "resolved_app_config")
load("//score/bazel/generators:franca2arxml.bzl", "franca2arxml_converter")

filegroup(
    name = "instance",
    srcs = [
        "bigdata.fcdl",
        "instance.cdepl",
        "instance.fcdl",
    ],
)

resolved_app_config(
    name = "app_config_resolved",
    srcs = ["app_config.json"],
    visibility = ["//ecu/xpad/xpad-shared/config/common:__pkg__"],
)

franca2arxml_converter(
    name = "arxml",
    srcs = [
        ":instance",
        "//ecu/xpad/xpad-shared/config/soc:boardnet_internal",
        "//ecu/xpad/xpad-shared/config/soc:device_deployment",
    ],
    app_config_files = [":app_config_resolved"],
    ecu_name = "BMW.Root_Ecu",
)

ara_arxml_inject_non_reporting_behavior(
    name = "patched_arxml",
    arxmls = [":arxml"],
)

ara_vector_artifacts(
    name = "amsrgen",
    arxmls = [
        ":patched_arxml",
        "//score/meta/datatypes:stdtypes",
    ],
)

filegroup_ara_vector_configs(
    name = "config_files",
    ara_vector_artifacts = ":amsrgen",
    component_name = "bigdata",
    visibility = [
        "//score/mw/com/test/bigdata:__pkg__",
    ],
)

filegroup(
    name = "bigdata-secpol",
    srcs = [
        "bigdata.secpol",
    ],
    visibility = [
        "//ecu/xpad/xpad-shared/config/common/os/bsw/qnx-secpol:__pkg__",
        "//ecu/xpad/xpad-shared/config/os:__subpackages__",
    ],
)
