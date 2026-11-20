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

# Certified-scope stubs for score_baselibs dependencies used by project components.
# Each stub declares the scope of one package group so the dependable_element
# validation does not emit "Not in certified scope" warnings.

score_baselibs_stub(
    name = "bitmanipulation",
    scope_path = "@score_baselibs//score/bitmanipulation:__subpackages__",
    visibility = ["//:__subpackages__"],
)

score_baselibs_stub(
    name = "concurrency",
    scope_path = "@score_baselibs//score/concurrency:__subpackages__",
    visibility = ["//:__subpackages__"],
)

score_baselibs_stub(
    name = "containers",
    scope_path = "@score_baselibs//score/containers:__subpackages__",
    visibility = ["//:__subpackages__"],
)

score_baselibs_stub(
    name = "filesystem",
    scope_path = "@score_baselibs//score/filesystem:__subpackages__",
    visibility = ["//:__subpackages__"],
)

score_baselibs_stub(
    name = "futurecpp",
    scope_path = "@score_baselibs//score/language/futurecpp:__subpackages__",
    visibility = ["//:__subpackages__"],
)

score_baselibs_stub(
    name = "json",
    scope_path = "@score_baselibs//score/json:__subpackages__",
    visibility = ["//:__subpackages__"],
)

score_baselibs_stub(
    name = "mw_log",
    scope_path = "@score_baselibs//score/mw/log:__subpackages__",
    visibility = ["//:__subpackages__"],
)

# scope covers score/os AND score/os/utils via __subpackages__
score_baselibs_stub(
    name = "os",
    scope_path = "@score_baselibs//score/os:__subpackages__",
    visibility = ["//:__subpackages__"],
)

score_baselibs_stub(
    name = "quality",
    scope_path = "@score_baselibs//score/quality:__subpackages__",
    visibility = ["//:__subpackages__"],
)

score_baselibs_stub(
    name = "result",
    scope_path = "@score_baselibs//score/result:__subpackages__",
    visibility = ["//:__subpackages__"],
)

score_baselibs_stub(
    name = "scope_exit",
    scope_path = "@score_baselibs//score/scope_exit:__subpackages__",
    visibility = ["//:__subpackages__"],
)

score_baselibs_stub(
    name = "static_reflection_visitor",
    scope_path = "@score_baselibs//score/static_reflection_with_serialization/visitor:__subpackages__",
    visibility = ["//:__subpackages__"],
)

score_baselibs_stub(
    name = "string_manipulation",
    scope_path = "@score_baselibs//score/string_manipulation:__subpackages__",
    visibility = ["//:__subpackages__"],
)

score_baselibs_stub(
    name = "string_view",
    scope_path = "@score_baselibs//score/language/safecpp/string_view:__subpackages__",
    visibility = ["//:__subpackages__"],
)

score_baselibs_stub(
    name = "utils",
    scope_path = "@score_baselibs//score/utils:__subpackages__",
    visibility = ["//:__subpackages__"],
)
