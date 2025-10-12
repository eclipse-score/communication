# *******************************************************************************
# Copyright (c) 2024 Contributors to the Eclipse Foundation
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

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def github_urls(path):
    return [
        "https://github.com/" + path,
    ]

def _lobster_impl(ctx):
    _VERSION = "0.14.1"
    _PATH = "bmw-software-engineering/lobster/archive/refs/tags/lobster-{version}.tar.gz".format(version = _VERSION)

    http_archive(
        name = "lobster",
        sha256 = "5a0b86c62cadc872dcb1b79485ba72953400bcdc42f97c5b5aefe210e92ce6ff",
        strip_prefix = "lobster-lobster-0.14.1",
        urls = github_urls(_PATH),
    )

def maybe(rule, **kwargs):
    """Like rule, but only calls rule if name is not already defined."""
    name = kwargs["name"]
    if name not in native.existing_rules():
        rule(**kwargs)

lobster_ext = module_extension(
    implementation = _lobster_impl,
)
