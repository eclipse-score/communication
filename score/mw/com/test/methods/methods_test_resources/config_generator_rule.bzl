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
    config_json = ctx.file.base_config_json_path
    generated_configs_out = ctx.actions.declare_file(ctx.attr.out_config_name)
    further_args = str(ctx.attr.further_args)

    ctx.actions.run(
        executable = ctx.executable.tool,
        inputs = [config_json],
        outputs = [generated_configs_out],
        arguments = [config_json.path, generated_configs_out.path, further_args],
    )

    return [DefaultInfo(files = depset([generated_configs_out]))]

make_config_json = rule(
    implementation = _make_configs,
    attrs = {
        "base_config_json_path": attr.label(allow_single_file = True),
        "out_config_name": attr.string(mandatory = True),
        "further_args": attr.string_dict(
            default = {},
            doc = "Any list of additional arguments that can be provider to the tool specified in the 'tool' attribute.",
        ),
        "tool": attr.label(
            default = "//score/mw/com/test/multiple_proxies/config_generator:config_generator",
            executable = True,
            cfg = "exec",
            allow_files = True,
        ),
    },
)

def _make_default_configs_impl(name, visibility, applicationID, asil_level_app, asil_level_communication_partner, **kwargs):
    out_config_name = "{}.json".format(name)

    make_config_json(
        name = name,
        base_config_json_path = "//score/mw/com/test/methods/methods_test_resources:base_mw_com_config_json",
        out_config_name = out_config_name,
        further_args = {
            "asil-level-app": asil_level_app,
            "asil-level-communication-partner": asil_level_communication_partner,
            "applicationID": applicationID,
        },
        tool = "//score/mw/com/test/methods/methods_test_resources:config_generator",
        visibility = visibility,
        **kwargs
    )

make_default_configs = macro(
    implementation = _make_default_configs_impl,
    attrs = {
        "applicationID": attr.string(configurable = False),
        "asil_level_app": attr.string(configurable = False, default = "QM"),
        "asil_level_communication_partner": attr.string(configurable = False, default = "QM"),
    },
)
