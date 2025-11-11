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
def _make_configs(ctx):
    config_json = ctx.file.config_json_path
    mw_com_config = ctx.file.mw_com_config_path

    out_name = ctx.attr.out_dir

    generated_configs_out = ctx.actions.declare_directory(out_name)

    ctx.actions.run(
        executable = ctx.executable._tool,
        inputs = [config_json, mw_com_config],
        outputs = [generated_configs_out],
        arguments = [config_json.path, mw_com_config.path, generated_configs_out.path],
    )

    return [DefaultInfo(files = depset([generated_configs_out]))]

make_configs = rule(
    implementation = _make_configs,
    attrs = {
        "config_json_path": attr.label(allow_single_file = True),
        "mw_com_config_path": attr.label(allow_single_file = True),
        "out_dir": attr.string(mandatory = True),
        "_tool": attr.label(
            default = "//score/mw/com/performance_benchmarks/macro_benchmark/config_generator:config_generator",
            executable = True,
            cfg = "exec",
            allow_files = True,
        ),
    },
)
