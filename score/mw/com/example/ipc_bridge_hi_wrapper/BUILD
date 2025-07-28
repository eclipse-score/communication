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
    name = "hi_app",
    srcs = [
        ":ipc_bridge_hi_wrapper",
        ":ipc_bridge_cpp_sil"
        ],
    outs = ["hi_app"],
    cmd = "cp $(location :ipc_bridge_hi_wrapper) $@ && \
     chmod ugo+w $@ && \
     $(execpath @gcc_toolchain_gcc//:elf-enabler) $@ && \
     chmod ugo-w $@",
    tools = ["@gcc_toolchain_gcc//:elf-enabler"],
)

cc_binary(
    name = "ipc_bridge_hi_wrapper",
    srcs = [
        "main.cc",
    ],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        "//score/mw/com/example/ipc_bridge:ipc_bridge_cpp",
    ],
)

genrule(
    name = "fetch-fastdev-archive",
    srcs = [],
    outs = ["fastdev-archive.tgz"],
    cmd = "wget -O $@ https://linux.elektrobit.com/tmp/score/fastdev-ubuntu-ebcl-deb-qemu-arm64.tgz"
)

genrule(
    name = "fastdev-image",
    srcs = [":fetch-fastdev-archive"],
    outs = [
        "deb-qemuarm64/fastdev-ubuntu-ebcl-deb-qemuarm64.wic",
        "deb-qemuarm64/fastdev-ubuntu-ebcl-deb-qemuarm64-vmlinux"
    ],
    cmd = "tar xzf $(location :fetch-fastdev-archive) -C $(RULEDIR)",
)

genrule(
    name = "upload",
    srcs = [
        ":ipc_bridge_cpp_sil",
        ":hi_app",
        "mw_com_config.json"
    ],
    cmd = " \
        sshpass -p linux scp -P 2222 $(location :ipc_bridge_cpp_sil) root@localhost:/usr/bin &&\
        sshpass -p linux scp -P 2222 $(location :hi_app) root@localhost:/usr/bin &&\
        sshpass -p linux scp -P 2222 $(location mw_com_config.json) root@localhost:/etc/ &&\
        echo 'Done' > $@\
    ",
    outs = ["upload.log"],
)
