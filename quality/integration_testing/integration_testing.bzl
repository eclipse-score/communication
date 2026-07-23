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

load("@rules_oci//oci:defs.bzl", "oci_image", "oci_load")
load("@rules_pkg//pkg:tar.bzl", "pkg_tar")
load("@score_itf//:defs.bzl", "py_itf_test")
load("@score_rules_imagefs//rules/qnx:ifs.bzl", "qnx_ifs")

def _extend_list_in_kwargs(kwargs, key, values):
    kwargs[key] = kwargs.get(key, []) + values
    return kwargs

def _extend_list_in_kwargs_without_duplicates(kwargs, key, values):
    kwargs_values = kwargs.get(key, [])
    for value in values:
        if value not in kwargs_values:
            kwargs_values.append(value)
    kwargs[key] = kwargs_values
    return kwargs

def integration_test(name, srcs, filesystem, extract_core = False, core_output_dir = None, **kwargs):
    image_name = "_image_{}".format(name)
    image_loader = "_image_{}_loader".format(name)
    repo_tag = "{}:latest".format(name)

    LINUX_TARGET_COMPATIBLE_WITH = select({
        "@platforms//cpu:x86_64": ["@platforms//cpu:x86_64"],
        "@platforms//cpu:arm64": ["@platforms//cpu:arm64"],
    }) + [
        "@platforms//os:linux",
    ]

    pkg_tar(
        name = "_oci_filesystem_{}".format(name),
        srcs = [filesystem],
    )

    oci_image(
        name = image_name,
        architecture = select({
            "@platforms//cpu:arm64": "arm64",
            "@platforms//cpu:x86_64": "amd64",
        }),
        os = "linux",
        env = select({
            "@score_cpp_policies//sanitizers/flags:any_sanitizer": "//quality/sanitizer:merged_absolute_env",
            "//conditions:default": None,
        }),
        tars = [
            "_oci_filesystem_{}".format(name),
        ] + select({
            "@score_cpp_policies//sanitizers/flags:any_sanitizer": ["//quality/sanitizer:suppressions_pkg"],
            "//conditions:default": [],
        }) + [
            "@ubuntu24_04_integration_testing//:ubuntu24_04_integration_testing",
        ],
        target_compatible_with = LINUX_TARGET_COMPATIBLE_WITH,
    )

    oci_load(
        name = image_loader,
        image = image_name,
        repo_tags = [repo_tag],
        target_compatible_with = LINUX_TARGET_COMPATIBLE_WITH,
    )

    QNX_TARGET_COMPATIBLE_WITH = select({
        "@platforms//cpu:x86_64": ["@platforms//cpu:x86_64"],
        "@platforms//cpu:arm64": ["@platforms//cpu:arm64"],
    }) + [
        "@platforms//os:qnx",
    ]

    qemu_image = "_init_ifs_{}".format(name)
    qnx_ifs(
        name = qemu_image,
        out = "init_ifs_{}".format(name),
        build_file = "//quality/integration_testing/environments/qnx8_qemu:init_build",
        srcs = [filesystem, "//quality/integration_testing/environments/qnx8_qemu:qnx_config"],
        target_compatible_with = QNX_TARGET_COMPATIBLE_WITH,
    )

    qemu_config = Label("//quality/integration_testing/environments/qnx8_qemu:qemu_config")

    _extend_list_in_kwargs(kwargs, "data", select({
        "//conditions:default": [image_loader],
        "@platforms//os:qnx": [qemu_image, qemu_config],
    }))
    _extend_list_in_kwargs(
        kwargs,
        "args",
        select({
            "//conditions:default": [
                "--log-cli-level=DEBUG",
                "--docker-image-bootstrap=$(location {})".format(image_loader),
                "--docker-image={}".format(repo_tag),
            ],
            "@platforms//os:qnx": [
                "--log-cli-level=DEBUG",
                "--qemu-config=$(location {})".format(qemu_config),
                "--qemu-image=$(location {})".format(qemu_image),
            ],
        }),
    )

    if extract_core:
        _extend_list_in_kwargs(kwargs, "args", ["--extract-core"])
        # Only pass --core-output-dir if explicitly specified by user
        # Otherwise, the plugin will use TEST_UNDECLARED_OUTPUTS_DIR/cores at runtime
        if core_output_dir != None:
            # Static validation only: filesystem checks are impossible here since
            # this runs at loading time on the build host, while the directory is
            # used at test runtime. The plugin creates the directory on demand.
            if not core_output_dir:
                fail("core_output_dir must be a non-empty path when specified")
            if not core_output_dir.startswith("/"):
                fail("core_output_dir must be an absolute path, got: {}".format(core_output_dir))
            if ".." in core_output_dir.split("/"):
                fail("core_output_dir must not contain '..' path segments, got: {}".format(core_output_dir))
            _extend_list_in_kwargs(kwargs, "args", ["--core-output-dir={}".format(core_output_dir)])
    elif core_output_dir != None:
        fail("core_output_dir was specified but extract_core is False; set extract_core = True to enable core extraction")

    # Tests spin up docker or qemu which requires a significant amount of system resources.
    if "size" not in kwargs:
        kwargs["size"] = "enormous"

    # While we require a significant amount of system resources, the tests are still short.
    if "timeout" not in kwargs:
        kwargs["timeout"] = "short"

    # FIXME: Integration tests are highly flaky with TSAN. (Ticket-249859)
    _extend_list_in_kwargs_without_duplicates(
        kwargs,
        "target_compatible_with",
        ["@score_cpp_policies//sanitizers/constraints:no_tsan"],
    )

    py_itf_test(
        name = name,
        srcs = srcs,
        plugins = select({
            "//conditions:default": [
                "@score_itf//score/itf/plugins:docker_plugin",
            ],
            "@platforms//os:qnx": [
                "@score_itf//score/itf/plugins:qemu_plugin",
            ],
        }),
        env = {
            "DOCKER_HOST": "",
            "TEST_UNDECLARED_OUTPUTS_DIR": "$(TEST_UNDECLARED_OUTPUTS_DIR)",
        },
        **kwargs
    )
