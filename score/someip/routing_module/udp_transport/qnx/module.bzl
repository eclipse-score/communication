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
"""Compiler flags for io-sock kernel modules, see the SDP's usr/include/devs/{mods.mk,opt_global.mk}."""

visibility("private")

_SDP_TARGET_DIRS = {
    "aarch64": "@score_qcc_aarch64_toolchain_pkg//:target_dir",
    "x86_64": "@score_qcc_x86_64_toolchain_pkg//:target_dir",
}

_QNX_KERNEL_FLAGS = [
    "-D__FreeBSD__",
    "-fno-builtin-log",
    "-fno-strict-aliasing",
    "-std=iso9899:1999",
    "-D__EXT",
    "-D_KERNEL",
    "-DHAVE_KERNEL_OPTION_HEADERS",
    "-DSMP",
    "-DEARLY_AP_STARTUP",
    "-DVIMAGE",
    "-DALTQ",
    "-DDEV_NETMAP",
    "-D_STRINGS_H_INCLUDED",
    "-D_UNISTD_H_INCLUDED",
    # The FreeBSD kernel headers are not warning clean.
    "-w",
]

_DEVS_SUBDIRS = ["", "/contrib/ck/include", "/qnx", "/qnx-gen", "/sys-nto"]

def qnx_sdp_inputs(qnx_x86_64, qnx_aarch64):
    """SDP target directory as compiler input, for the given config_settings.

    Args:
      qnx_x86_64: config_setting for QNX x86_64.
      qnx_aarch64: config_setting for QNX aarch64.
    Returns:
      select() with the label of the SDP target directory.
    """
    return select({
        qnx_x86_64: [_SDP_TARGET_DIRS["x86_64"]],
        qnx_aarch64: [_SDP_TARGET_DIRS["aarch64"]],
        "//conditions:default": [],
    })

def _copts(cpu):
    devs = "$(execpath {})/usr/include/devs".format(_SDP_TARGET_DIRS[cpu])
    includes = []
    for subdir in _DEVS_SUBDIRS + ["/include_" + cpu]:
        includes += ["-isystem", devs + subdir]
    return _QNX_KERNEL_FLAGS + includes

def qnx_copts(qnx_x86_64, qnx_aarch64):
    """Defines and include paths of the SDP's FreeBSD kernel headers, for the given config_settings.

    Args:
      qnx_x86_64: config_setting for QNX x86_64.
      qnx_aarch64: config_setting for QNX aarch64.
    Returns:
      select() with the compiler flags.
    """
    return select({
        qnx_x86_64: _copts("x86_64"),
        qnx_aarch64: _copts("aarch64"),
        "//conditions:default": [],
    })
