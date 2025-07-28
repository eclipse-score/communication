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
        "//score/mw/com/example/ipc_bridge:ipc_bridge_cpp",
        ":hi_app",
        ":fastdev-image",
        "mw_com_config.json",
        "run_qemu.sh"
    ],
    cmd = " \
        mkdir -p $(RULEDIR)/deb-qemuarm64-modified &&\
        cp $(RULEDIR)/deb-qemuarm64/* $(RULEDIR)/deb-qemuarm64-modified/ &&\
        $(location run_qemu.sh) $(RULEDIR)/deb-qemuarm64-modified -pidfile $(RULEDIR)/qemu.pid > $(RULEDIR)/qemu_upload.log &\
        sleep 10 ; \
        sshpass -p linux scp -o StrictHostKeyChecking=no -P 2222 $(location :ipc_bridge_cpp_sil) root@localhost:/usr/bin &&\
        sshpass -p linux scp -o StrictHostKeyChecking=no -P 2222 $(location //score/mw/com/example/ipc_bridge:ipc_bridge_cpp) root@localhost:/usr/bin &&\
        sshpass -p linux scp -o StrictHostKeyChecking=no -P 2222 $(location :hi_app) root@localhost:/usr/bin &&\
        sshpass -p linux scp -o StrictHostKeyChecking=no -P 2222 $(location mw_com_config.json) root@localhost:/etc/ &&\
        sshpass -p linux ssh -o StrictHostKeyChecking=no -p 2222 root@localhost sync &&\
        sshpass -p linux ssh -o StrictHostKeyChecking=no -p 2222 root@localhost crinit-ctl poweroff || true \
        sleep 5  \
    ",
    outs = [
        "qemu_upload.log",
        "deb-qemuarm64-modified/fastdev-ubuntu-ebcl-deb-qemuarm64.wic",
        "deb-qemuarm64-modified/fastdev-ubuntu-ebcl-deb-qemuarm64-vmlinux"
    ],
)

genrule(
    name = "run_example",
    srcs = [
        "deb-qemuarm64-modified/fastdev-ubuntu-ebcl-deb-qemuarm64.wic",
        "deb-qemuarm64-modified/fastdev-ubuntu-ebcl-deb-qemuarm64-vmlinux",
        "run_qemu.sh"
    ],
    cmd = " \
        $(location run_qemu.sh) $(RULEDIR)/deb-qemuarm64-modified -pidfile $(RULEDIR)/qemu.pid > $(RULEDIR)/qemu_run.log &\
        sleep 10 ; \
        sshpass -p linux ssh -o StrictHostKeyChecking=no -p 2222 root@localhost ipc_bridge_cpp -n 10 -m send -t 200 -s /etc/mw_com_config.json > $(RULEDIR)/ssh_run.log && \
        sshpass -p linux ssh -o StrictHostKeyChecking=no -p 2222 root@localhost crinit-ctl poweroff || true \
        sleep 5 \
    ",
    outs = [
        "qemu_run.log",
        "ssh_run.log",
    ],
)
