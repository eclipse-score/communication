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
    name = "data_slots_read_only_test",
    srcs = [
        "__init__.py",
        "data_slots_read_only_test.py",
    ],
    data = [
        "//score/mw/com/test/data_slots_read_only:data_slots_read_only-pkg",
    ],
    main = "data_slots_read_only_test.py",
    module_root = "score/mw/com/test/data_slots_read_only/sct",
)
