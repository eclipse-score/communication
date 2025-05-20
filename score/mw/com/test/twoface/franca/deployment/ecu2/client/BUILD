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

load("//score/bazel/generators:franca2arxml.bzl", "franca2arxml_converter")

filegroup(
    name = "instance",
    srcs = [
        "instance.cdepl",
        "instance.fcdl",
        "//score/mw/com/test/twoface/franca/application:client_app",
    ],
)

franca2arxml_converter(
    name = "arxmls_20-11",
    srcs = [
        ":instance",
        "//score/mw/com/test/twoface/franca/deployment/ecu2:device_deployment",
        "//score/mw/com/test/twoface/franca/system_description:Ecu2",
    ],
    ecu_name = "bmw.components.Ecu2",
    visibility = ["//score/mw/com/test/twoface:__subpackages__"],
)
