#!/usr/bin/env bash
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
#
# Verifies the developer machine setup described in CONTRIBUTING.md.
#
# Invoked from tools/bazel on every bazel invocation (via bazelisk's
# tools/bazel hook). Checks:
# - On Linux, that Bazel's own `linux-sandbox` helper actually works, warning
#   if not since that means every sandboxed action silently falls back to a
#   weaker, insufficient sandbox.
# - Docker is installed *and usable by the current user* (i.e. `docker info`
#   succeeds), since integration tests need it.
#
# Intentionally only emits warnings. Just because this check deems a system
# incompatible, it does not mean that we can block out users. We just warn
# them about potential errors, to help them correctly set up their machine.

set -u

warnings=()

bazel_real="${BAZEL_REAL:-bazel}"

# Color warnings when writing to an interactive terminal, respecting the
# NO_COLOR convention (https://no-color.org/). Never colorize otherwise
# (e.g. when output is redirected/piped into a log file).
color_yellow=""
color_bold=""
color_reset=""
if [ -t 2 ] && [ -z "${NO_COLOR:-}" ]; then
    color_yellow="$(printf '\033[33m')"
    color_bold="$(printf '\033[1m')"
    color_reset="$(printf '\033[0m')"
fi

check_linux_sandbox() {
    local install_base sandbox_bin

    install_base="$("${bazel_real}" info install_base 2>/dev/null)"
    if [ -z "${install_base}" ]; then
        warnings+=("Could not determine Bazel's install_base to verify linux-sandbox; skipping that check.")
        return
    fi

    sandbox_bin="${install_base}/linux-sandbox"
    if [ ! -x "${sandbox_bin}" ]; then
        warnings+=("${sandbox_bin}: not found; Sandboxing is potentially insufficient.")
        return
    fi

    if ! "${sandbox_bin}" /bin/false >/dev/null 2>&1; then
        warnings+=("\`${sandbox_bin} /bin/true\` failed.
This means Bazel silently falls back to a potentially insufficient sandbox.")
    fi
}

check_docker() {
    local docker_info_output

    if ! command -v docker >/dev/null 2>&1; then
        warnings+=("docker not found on PATH. Some integration tests require it. Install: https://docs.docker.com/engine/install/")
        return
    fi

    if docker_info_output="$(docker info 2>&1 1>/dev/null)"; then
        return
    fi

    docker_info_output="$(printf '%s' "${docker_info_output}" | tr '[:upper:]' '[:lower:]')"
    if printf '%s' "${docker_info_output}" | grep -q "permission denied"; then
        warnings+=("docker is installed but the current user cannot talk to the Docker daemon (permission denied). Add yourself to the \`docker\` group (\`sudo usermod -aG docker \$USER\`), then log out and back in (or \`newgrp docker\`), and retry. See https://docs.docker.com/engine/install/linux-postinstall/")
    elif printf '%s' "${docker_info_output}" | grep -qE "cannot connect|daemon is running|no such file"; then
        warnings+=("docker is installed but its daemon doesn't appear to be running/reachable (\`docker info\` failed). Start the Docker service, e.g. \`sudo systemctl start docker\`, and retry.")
    else
        warnings+=("docker is installed but \`docker info\` failed.")
    fi
}

if [ "$(uname -s)" = "Linux" ]; then
    check_linux_sandbox
fi

check_docker

if [ "${#warnings[@]}" -gt 0 ]; then
    for warning in "${warnings[@]}"; do
        printf '\n%s%s%s\n' "${color_yellow}" "${warning}" "${color_reset}" >&2
    done
    printf '\n%s%sThis machine is not compliant with the instructions in CONTRIBUTING.md.\nThis can cause unexpected failures. Before filing any bug report, please follow the instructions in CONTRIBUTING.md%s\n' "${color_bold}" "${color_yellow}" "${color_reset}" >&2
fi

exit 0
