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
    name = "subscribe_handler_test",
    srcs = [
        "__init__.py",
        "subscribe_handler_test.py",
    ],
    data = ["//score/mw/com/test/subscribe_handler:subscribe_handler-pkg.tar"],
    main = "subscribe_handler_test.py",
    module_root = "score/mw/com/test/subscribe_handler/sct",
)
