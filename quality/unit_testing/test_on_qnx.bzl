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

"""Build a (test) target for the QNX target platform regardless of the host platform.

QNX-only tests carry `target_compatible_with = ["@platforms//os:qnx"]` and are
therefore incompatible when the target platform is the Linux host. Depending on
such a test from a non-QNX target (e.g. a `dependable_element` feeding the Sphinx
documentation) makes that target incompatible as well and breaks the build.

`on_qnx_test` wraps such a dependency in an outgoing edge transition to the QNX
platform, so the wrapped target is always analysed as compatible. The QNX
toolchains are registered in MODULE.bazel, hence only the platform needs to be
switched here.
"""

visibility(["//..."])

_QNX_PLATFORM = "@score_bazel_platforms//:x86_64-qnx-sdp_8.0.0-posix"

def _test_on_qnx_transition_impl(settings, attr):
    return {
        "//command_line_option:platforms": _QNX_PLATFORM,
    }

_test_on_qnx_transition = transition(
    implementation = _test_on_qnx_transition_impl,
    inputs = [],
    outputs = ["//command_line_option:platforms"],
)

def _test_on_qnx_impl(ctx):
    executable = ctx.actions.declare_file(ctx.label.name)
    ctx.actions.write(
        output = executable,
        content = "#!/bin/sh\nexit 0\n",
        is_executable = True,
    )
    return [DefaultInfo(executable = executable)]

on_qnx_test = rule(
    implementation = _test_on_qnx_impl,
    doc = "Depends on `actual` built for the QNX platform and reports success.",
    test = True,
    attrs = {
        "actual": attr.label(
            mandatory = True,
            cfg = _test_on_qnx_transition,
        ),
        "_allowlist_function_transition": attr.label(
            default = "@bazel_tools//tools/allowlists/function_transition_allowlist",
        ),
    },
)
