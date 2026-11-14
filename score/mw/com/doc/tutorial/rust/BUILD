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

# Documentation sources of the Rust tutorial, including the files referenced via literalinclude.
filegroup(
    name = "doc_files",
    srcs = [
        "README.rst",
        "//score/mw/com/doc/tutorial/rust/common:doc_files",
    ] + [
        "//score/mw/com/doc/tutorial/rust/chapter_%d:doc_files" % i
        for i in range(1, 9)
    ],
    visibility = ["//score/mw/com/doc/tutorial:__pkg__"],
)
