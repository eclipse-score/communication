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

load("//score/tools/sctf/bazel_gen:sctf_gen.bzl", "py_sctf_test")

py_sctf_test(
    name = "field_initial_value_test",
    srcs = [
        "__init__.py",
        "field_initial_value_test.py",
    ],
    data = [
        "//score/mw/com/test/field_initial_value:client-pkg",
        "//score/mw/com/test/field_initial_value:service-pkg",
    ],
    main = "field_initial_value_test.py",
    module_root = "score/mw/com/test/field_initial_value/sct",
)
