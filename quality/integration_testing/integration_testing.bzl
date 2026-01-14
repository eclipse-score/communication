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

load("@rules_oci//oci:defs.bzl", "oci_image", "oci_tarball")
load("@rules_python//python:defs.bzl", "py_test")
load("@score_communication_pip//:requirements.bzl", "requirement")
load("@score_toolchains_qnx//rules/fs:ifs.bzl", "qnx_ifs")
load("//quality/integration_testing/environments:run_as_exec.bzl", "test_as_exec")

def _extend_list_in_kwargs(kwargs, key, values):
    kwargs[key] = kwargs.get(key, []) + values
    return kwargs

def integration_test(name, srcs, filesystem, **kwargs):
    pytest_bootstrap = Label("//quality/integration_testing:main.py")
    image_name = "_image_{}".format(name)
    image_tarball = "_image_{}_tarball".format(name)
    repo_tag = "{}:latest".format(name)

    LINUX_TARGET_COMPATIBLE_WITH = select({
        "@platforms//cpu:x86_64": ["@platforms//cpu:x86_64"],
        "@platforms//cpu:arm64": ["@platforms//cpu:arm64"],
    }) + [
        "@platforms//os:linux",
    ]

    oci_image(
        name = image_name,
        architecture = select({
            "@platforms//cpu:arm64": "arm64",
            "@platforms//cpu:x86_64": "amd64",
        }),
        os = "linux",
        tars = [
            filesystem,
            "@ubuntu24_04//:ubuntu24_04",
        ],
        target_compatible_with = LINUX_TARGET_COMPATIBLE_WITH,
    )

    oci_tarball(
        name = image_tarball,
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
        tars = {"FOLDER": filesystem},
        target_compatible_with = QNX_TARGET_COMPATIBLE_WITH,
    )

    _extend_list_in_kwargs(kwargs, "data", select({
        "//conditions:default": [image_tarball],
        "@platforms//os:qnx": [qemu_image],
    }))
    _extend_list_in_kwargs(
        kwargs,
        "args",
        select({
            "//conditions:default": [
                "--docker-image-bootstrap=$(location {})".format(image_tarball),
                "--docker-image={}".format(repo_tag),
                "-p no:cacheprovider",
                "-p quality.integration_testing.environments.ubuntu24_04_docker.docker",
            ],
            "@platforms//os:qnx": [
                "--qemu_image=$(location {})".format(qemu_image),
                "-p no:cacheprovider",
                "-p quality.integration_testing.environments.qnx8_qemu.qemu",
            ],
        }),
    )

    py_test(
        name = "_test_internal_docker_{}".format(name),
        srcs = [
            pytest_bootstrap,
        ] + srcs,
        main = pytest_bootstrap,
        deps = [
            requirement("pytest"),
            "//quality/integration_testing/environments/ubuntu24_04_docker:docker",
        ],
        target_compatible_with = LINUX_TARGET_COMPATIBLE_WITH,
        tags = ["manual"],
        **kwargs
    )

    py_test(
        name = "_test_internal_qemu_{}".format(name),
        srcs = [
            pytest_bootstrap,
        ] + srcs,
        main = pytest_bootstrap,
        deps = [
            requirement("pytest"),
            "//quality/integration_testing/environments/qnx8_qemu:qemu",
        ],
        target_compatible_with = LINUX_TARGET_COMPATIBLE_WITH,
        tags = ["manual"],
        **kwargs
    )

    test_as_exec(
        name = name,
        executable = select({
            "//conditions:default": "_test_internal_docker_{}".format(name),
            "@platforms//os:qnx": "_test_internal_qemu_{}".format(name),
        }),
        **kwargs
    )
