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

"""Macro packaging an application together with its configuration files."""

load("@rules_pkg//pkg:mappings.bzl", "pkg_attributes", "pkg_filegroup", "pkg_files")

visibility(["//..."])

def pkg_application(name, app_name, bin = [], bin_name = None, etc = [], etc_deps = [], **kwargs):
    """Packages a binary + its etc files under /opt/{app_name}.

    Args:
        bin_name: optional new filename for the (single) binary in `bin`, e.g. so two
            packages built from the same cc_binary end up with distinct installed
            names (which also gives them distinct log tags in the ITF harness, since
            it tags output by the basename of the executed binary path).
    """
    pkg_files(
        name = "{}_binary".format(name),
        srcs = bin,
        prefix = "/opt/{}/bin".format(app_name),
        renames = {bin[0]: bin_name} if bin_name else {},
        attributes = pkg_attributes(
            mode = "0777",
        ),
        **kwargs
    )

    pkg_files(
        name = "{}_etc_files".format(name),
        srcs = etc,
        **kwargs
    )

    pkg_filegroup(
        name = "{}_etc".format(name),
        srcs = [
            "{}_etc_files".format(name),
        ] + etc_deps,
        prefix = "/opt/{}/etc".format(app_name),
    )

    pkg_filegroup(
        name = name,
        srcs = [
            "{}_binary".format(name),
            "{}_etc".format(name),
        ],
        prefix = "/",
        **kwargs
    )
