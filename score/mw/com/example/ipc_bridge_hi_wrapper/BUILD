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
load("@score-baselibs//score/language/safecpp:toolchain_features.bzl", "COMPILER_WARNING_FEATURES")


genrule(
    name = "ipc_bridge_cpp_sil",
    srcs = ["//score/mw/com/example/ipc_bridge:ipc_bridge_cpp"],
    outs = ["ipc_bridge_cpp_sil"],
    cmd = "cp $(SRCS) $@ && \
     chmod ugo+w $@ && \
     $(execpath @gcc_toolchain_gcc//:elf-enabler) $@ && \
     chmod ugo-w $@",
    tools = ["@gcc_toolchain_gcc//:elf-enabler"],
)

genrule(
    name = "hi_main",
    srcs = [
        ":ipc_bridge_hi_wrapper",
        ":ipc_bridge_cpp_sil"
        ],
    outs = ["hi_main"],
    cmd = "cp $(location :ipc_bridge_hi_wrapper) $@ && \
     chmod ugo+w $@ && \
     $(execpath @gcc_toolchain_gcc//:elf-enabler) $@ && \
     chmod ugo-w $@",
    tools = ["@gcc_toolchain_gcc//:elf-enabler"],
)

cc_binary(
    name = "ipc_bridge_hi_wrapper",
    srcs = [
        "main.c",
    ],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        "//score/mw/com/example/ipc_bridge:ipc_bridge_cpp",
    ],
)
