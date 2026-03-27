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

"""
Runs an executable on execution platform, with runtime deps built with either
target or execution platform configuration.

Executable rule outputs a symlink to the actual executable, fully leveraging
it's implementation (e.g. reuse a py_binary) and remaining somewhat OS
independent.

NOTE: This has one caveat, that the symlink is "built" with target
configuration. So do not depend on targets output by this rule unless
you're qualified personnel.
"""

def run_as_exec(**kwargs):
    """This macro remaps the arguments for clarity:

    deps -> data_as_exec: Runtime deps built with execution platform configuration.
    """
    data_as_exec = kwargs.pop("data_as_exec", None)
    _as_exec_run(
        deps = data_as_exec,
        **kwargs
    )

def test_as_exec(**kwargs):
    """This macro remaps the arguments for clarity:

    deps -> data_as_exec: Runtime deps built with execution platform configuration.
    """
    data_as_exec = kwargs.pop("data_as_exec", None)
    _as_exec_test(
        deps = data_as_exec,
        **kwargs
    )

_RULE_ATTRS = {
    # In order for args expansion to work in bazel for an executable rule
    # the attributes must be one of: "srcs", "deps", "data" or "tools".
    # See Bazel's LocationExpander implementation, these attribute names
    # are hardcoded.
    "data": attr.label_list(
        allow_files = True,
        cfg = "target",
    ),
    "deps": attr.label_list(
        allow_files = True,
        cfg = "exec",
    ),
    "env": attr.string_dict(),
    "executable": attr.label(
        allow_files = True,
        cfg = "exec",
        executable = True,
        mandatory = True,
    ),
}

def _executable_as_exec_impl(ctx):
    link = ctx.actions.declare_file(ctx.attr.name)
    ctx.actions.symlink(
        output = link,
        target_file = ctx.executable.executable,
        is_executable = True,
    )

    return [
        DefaultInfo(
            executable = link,
            runfiles = ctx.runfiles(
                files = ctx.files.data + ctx.files.deps + ctx.files.executable,
                transitive_files = depset(
                    transitive = [ctx.attr.executable.default_runfiles.files] +
                                 [dataf.default_runfiles.files for dataf in ctx.attr.data] +
                                 [dataf.data_runfiles.files for dataf in ctx.attr.data],
                ),
            ),
        ),
        RunEnvironmentInfo(environment = ctx.attr.env),
    ]

_as_exec_run = rule(
    implementation = _executable_as_exec_impl,
    attrs = _RULE_ATTRS,
    executable = True,
)

_as_exec_test = rule(
    implementation = _executable_as_exec_impl,
    attrs = _RULE_ATTRS,
    test = True,
)
