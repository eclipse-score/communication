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

visibility(["//:__pkg__"])

"""Repository rule that verifies a developer machine is set up per CONTRIBUTING.md.

- On Linux, actually try to run Bazel's own ``linux-sandbox`` helper,
  warning if it fails since that means every sandboxed action silently
  falls back to a weaker, insufficient sandbox.
- Docker should be installed *and usable by the current user* (i.e.
  ``docker info`` succeeds) for the integration tests that need it.

Intentionally only emits warnings. Just because this check deems a system incompatible,
it does not mean that we can block out users. We just warn them about potential errors.
This helps them with correctly setting up their machine.

Reexecutes on every bazel invocation. When modifying this code make sure it remains performant.
"""

def _bazel_install_base(repository_ctx):
    """Returns Bazel's install_base, or None if it can't be determined.

    Cannot simply invoke `bazel info install_base` against the *currently
    running* server/output_base: that recurses into the very server that is
    executing this repository rule and can hang or crash it.
    Instead, spawn a throwaway `--batch` Bazel client pointed at
    a scratch `--output_base`, which is independent of the running server
    yet resolves to the identical install_base.
    """
    bazel = repository_ctx.which("bazel")
    if bazel == None:
        return None

    root = str(repository_ctx.workspace_root)
    result = repository_ctx.execute([
        "sh",
        "-c",
        "OB=\"$(mktemp -d)\" && trap 'rm -rf \"$OB\"' EXIT && " +
        "cd '%s' && bazel --batch --output_base=\"$OB\" info install_base" % root,
    ])
    if result.return_code != 0:
        return None
    lines = [l for l in result.stdout.splitlines() if l.strip()]
    return lines[-1].strip() if lines else None

def _docker_usability_warning(repository_ctx):
    """Returns a warning string if docker isn't installed/usable, else None."""
    docker = repository_ctx.which("docker")
    if docker == None:
        return (
            "docker not found on PATH. Some integration tests require it. " +
            "Install: https://docs.docker.com/engine/install/"
        )

    result = repository_ctx.execute([docker, "info"])
    if result.return_code == 0:
        return None

    stderr = result.stderr.strip().lower()
    if "permission denied" in stderr:
        return (
            "docker is installed but the current user cannot talk to the Docker daemon " +
            "(permission denied). Add yourself to the `docker` group " +
            "(`sudo usermod -aG docker $USER`), then log out and back in (or `newgrp " +
            "docker`), and retry. See " +
            "https://docs.docker.com/engine/install/linux-postinstall/"
        )
    if "cannot connect" in stderr or "daemon is running" in stderr or "no such file" in stderr:
        return (
            "docker is installed but its daemon doesn't appear to be running/reachable " +
            "(`docker info` failed: {}). Start the Docker service, e.g. `sudo systemctl " +
            "start docker`, and retry."
        ).format(result.stderr.strip())
    return "docker is installed but `docker info` failed: {}".format(
        result.stderr.strip() or "exit {}".format(result.return_code),
    )

def _dev_setup_check_impl(repository_ctx):
    warnings = []

    if repository_ctx.os.name.startswith("linux"):
        install_base = _bazel_install_base(repository_ctx)
        if install_base == None:
            warnings.append(
                "Could not determine Bazel's install_base to verify linux-sandbox; skipping that check.",
            )
        else:
            sandbox_bin = install_base + "/linux-sandbox"
            if not repository_ctx.path(sandbox_bin).exists:
                warnings.append(
                    "{}: not found; Sandboxing is potentially insufficient.".format(sandbox_bin),
                )
            else:
                smoke_test = repository_ctx.execute([sandbox_bin, "/bin/true"])
                if smoke_test.return_code != 0:
                    warnings.append((
                        "`{} /bin/true` failed (exit {}).\n" +
                        "This means Bazel silently falls back to a potentially insufficient sandbox."
                    ).format(sandbox_bin, smoke_test.return_code))

    docker_warning = _docker_usability_warning(repository_ctx)
    if docker_warning != None:
        warnings.append(docker_warning)

    for warning in warnings:
        # buildifier: disable=print
        print(
            "\n" +
            warning,
        )

    if warnings != []:
        print(
            "\n" +
            "This machine is not compliant with the instructions in CONTRIBUTING.md.\n" +
            "This can cause unexpected failures. " +
            "Before filing any bug report, please follow the instructions in CONTRIBUTING.md",
        )

    repository_ctx.file("BUILD", "")
    repository_ctx.file("defs.bzl", "DEV_SETUP_OK = True\n")

dev_setup_check = repository_rule(
    implementation = _dev_setup_check_impl,
    local = True,
    doc = "Verifies the developer machine setup described in CONTRIBUTING.md.",
)
