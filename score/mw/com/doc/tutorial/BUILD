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

load("@rules_python//sphinxdocs:sphinx_docs_library.bzl", "sphinx_docs_library")

# RST documentation files and the source files they reference via literalinclude.
# Each chapter is a separate Bazel package so its files are gathered via
# per-chapter filegroup targets and listed here explicitly.
sphinx_docs_library(
    name = "tutorial_rst",
    srcs = [
        "README.rst",
        "//score/mw/com/doc/tutorial/chapter_1:doc_files",
        "//score/mw/com/doc/tutorial/chapter_10:doc_files",
        "//score/mw/com/doc/tutorial/chapter_11:doc_files",
        "//score/mw/com/doc/tutorial/chapter_2:doc_files",
        "//score/mw/com/doc/tutorial/chapter_3:doc_files",
        "//score/mw/com/doc/tutorial/chapter_4:doc_files",
        "//score/mw/com/doc/tutorial/chapter_5:doc_files",
        "//score/mw/com/doc/tutorial/chapter_6:doc_files",
        "//score/mw/com/doc/tutorial/chapter_7:doc_files",
        "//score/mw/com/doc/tutorial/chapter_8:doc_files",
        "//score/mw/com/doc/tutorial/chapter_9:doc_files",
    ],
    prefix = "docs/sphinx/tutorial/",
    visibility = ["//docs/sphinx:__pkg__"],
)

# All the tutorial samples from the various chapters
filegroup(
    name = "tutorial_samples",
    srcs = [
        "//score/mw/com/doc/tutorial/chapter_1",
        "//score/mw/com/doc/tutorial/chapter_10",
        "//score/mw/com/doc/tutorial/chapter_11",
        "//score/mw/com/doc/tutorial/chapter_2",
        "//score/mw/com/doc/tutorial/chapter_3",
        "//score/mw/com/doc/tutorial/chapter_4",
        "//score/mw/com/doc/tutorial/chapter_5",
        "//score/mw/com/doc/tutorial/chapter_6",
        "//score/mw/com/doc/tutorial/chapter_7",
        "//score/mw/com/doc/tutorial/chapter_8",
        "//score/mw/com/doc/tutorial/chapter_9",
    ],
    visibility = ["//score/mw/com/doc:__subpackages__"],
)
