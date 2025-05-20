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
    name = "separate_reception_threads_test",
    timeout = "short",
    srcs = [
        "__init__.py",
        "separate_reception_threads_test.py",
    ],
    data = ["//score/mw/com/test/separate_reception_threads:separate_reception_threads-pkg.tar"],
    main = "separate_reception_threads_test.py",
    module_root = "score/mw/com/test/separate_reception_threads/sct",
)
