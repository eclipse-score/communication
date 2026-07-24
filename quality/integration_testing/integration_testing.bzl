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

def integration_test(name, srcs, filesystem, **kwargs):
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
        env = {"DOCKER_HOST": ""},
        **kwargs
    )

def dual_qemu_integration_test(
        name,
        srcs,
        filesystem_a,
        filesystem_b = None,
        dual_config = Label("//quality/integration_testing/environments/dual_qemu:dual_qemu_config.example.json"),
        **kwargs):
    """Run an integration test on TWO QNX QEMU VMs sharing an ivshmem region.

    Mirrors `integration_test` but wires the `dual_qemu` plugin (which provides the
    `target_a` / `target_b` fixtures) instead of the single-VM `qemu_plugin`. Each VM boots
    its own IFS image built from `filesystem_a` / `filesystem_b`. When `filesystem_b` is not
    specified both VMs share the same image. QNX-only.

    Args:
        name: test target name.
        srcs: pytest source files.
        filesystem_a: a `pkg_*` target installed into VM-A's IFS image.
        filesystem_b: a `pkg_*` target installed into VM-B's IFS image. Defaults to
            ``filesystem_a`` (both VMs boot the same image).
        dual_config: JSON config describing the two VMs and the ivshmem region.
        **kwargs: forwarded to `py_itf_test`.
    """
    if filesystem_b == None:
        filesystem_b = filesystem_a

    QNX_TARGET_COMPATIBLE_WITH = select({
        "@platforms//cpu:x86_64": ["@platforms//cpu:x86_64"],
        "@platforms//cpu:arm64": ["@platforms//cpu:arm64"],
    }) + [
        "@platforms//os:qnx",
    ]

    qemu_image_a = "_init_ifs_{}_a".format(name)
    qnx_ifs(
        name = qemu_image_a,
        out = "init_ifs_{}_a".format(name),
        build_file = "//quality/integration_testing/environments/qnx8_qemu:init_build",
        srcs = [filesystem_a, "//quality/integration_testing/environments/qnx8_qemu:qnx_config"],
        target_compatible_with = QNX_TARGET_COMPATIBLE_WITH,
    )

    qemu_image_b = "_init_ifs_{}_b".format(name)
    qnx_ifs(
        name = qemu_image_b,
        out = "init_ifs_{}_b".format(name),
        build_file = "//quality/integration_testing/environments/qnx8_qemu:init_build",
        srcs = [filesystem_b, "//quality/integration_testing/environments/qnx8_qemu:qnx_config"],
        target_compatible_with = QNX_TARGET_COMPATIBLE_WITH,
    )

    _extend_list_in_kwargs(kwargs, "data", [qemu_image_a, qemu_image_b, dual_config])
    _extend_list_in_kwargs(
        kwargs,
        "args",
        [
            "--log-cli-level=DEBUG",
            "--dual-qemu-config=$(location {})".format(dual_config),
            "--qemu-image-a=$(location {})".format(qemu_image_a),
            "--qemu-image-b=$(location {})".format(qemu_image_b),
        ],
    )

    # Two VMs require even more resources than a single one.
    if "size" not in kwargs:
        kwargs["size"] = "enormous"
    if "timeout" not in kwargs:
        kwargs["timeout"] = "moderate"

    # Driving two real QNX guests under KVM has rare, environment-induced boot
    # nondeterminism (e.g. a guest occasionally wedging during device bring-up).
    # The fixtures already stagger boots and wait for stable SSH; mark the test
    # flaky so bazel transparently retries such infrastructure hiccups.
    if "flaky" not in kwargs:
        kwargs["flaky"] = True

    _extend_list_in_kwargs_without_duplicates(
        kwargs,
        "target_compatible_with",
        ["@score_cpp_policies//sanitizers/constraints:no_tsan"],
    )
    _extend_list_in_kwargs(kwargs, "target_compatible_with", QNX_TARGET_COMPATIBLE_WITH)

    py_itf_test(
        name = name,
        srcs = srcs,
        plugins = [
            "//quality/integration_testing/environments/dual_qemu:dual_qemu_plugin",
        ],
        **kwargs
    )
