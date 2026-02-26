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

load("@rules_pkg//pkg:mappings.bzl", "pkg_attributes", "pkg_filegroup", "pkg_files")
load("@rules_pkg//pkg:tar.bzl", "pkg_tar")

def pkg_application(name, app_name, bin = [], etc = [], etc_deps = [], **kwargs):
    pkg_files(
        name = "{}_binary".format(name),
        srcs = bin,
        prefix = "/opt/{}/bin".format(app_name),
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

    pkg_tar(
        name = name,
        srcs = [
            "{}_binary".format(name),
            "{}_etc".format(name),
        ],
        **kwargs
    )
