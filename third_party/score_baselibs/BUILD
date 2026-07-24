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

load(":score_baselibs_stub.bzl", "score_baselibs_stub")

exports_files(["restore_qnx8_poll_workaround.patch"])

# Certified-scope stubs for score_baselibs dependencies used by mw_com.
# Each stub declares the scope of one package group so the dependable_element
# validation does not emit "Not in certified scope" warnings.

score_baselibs_stub(
    name = "containers",
    scope_path = "@@score_baselibs+//score/containers:__subpackages__",
    visibility = ["//:__subpackages__"],
)

score_baselibs_stub(
    name = "futurecpp",
    scope_path = "@@score_baselibs+//score/language/futurecpp:__subpackages__",
    visibility = ["//:__subpackages__"],
)

# scope covers score/os AND score/os/utils via __subpackages__
score_baselibs_stub(
    name = "os",
    scope_path = "@@score_baselibs+//score/os:__subpackages__",
    visibility = ["//:__subpackages__"],
)

score_baselibs_stub(
    name = "bitmanipulation",
    scope_path = "@@score_baselibs+//score/bitmanipulation:__subpackages__",
    visibility = ["//:__subpackages__"],
)
