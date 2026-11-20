#!/bin/bash

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

# Creates the tap device the qemu benchmark targets attach to.
# The ITF qemu plugin passes `script=no`, so qemu attaches to an existing device and never creates one.
# The device does not survive a host reboot, so this runs once per boot:
#   bazel run //score/lg-someip/tests/benchmark/test_cases:setup_tap
#
# IFACE is networks[0].name and HOST_CIDR is networks[0].gateway of config/qemu_config.json: the host side of the
# link, where the someipy peer binds.
set -euo pipefail

IFACE="tap0"
HOST_CIDR="10.0.2.2/24"

if [[ $(id -u) -ne 0 ]]; then
    echo "This needs root to create a tap device; re-running under sudo." >&2
    exec sudo -- "$0" "$@"
fi

# The device belongs to whoever will run the tests, not to root: qemu runs unprivileged under bazel.
OWNER="${SUDO_USER:-$(id -un)}"

# Recreate rather than reuse: a persistent tap keeps whatever flags the last qemu to attach left on it.
if ip link show "${IFACE}" >/dev/null 2>&1; then
    echo "Removing the existing ${IFACE} so its flags cannot carry over."
    ip link delete "${IFACE}"
fi

ip tuntap add "${IFACE}" mode tap user "${OWNER}"
ip addr add "${HOST_CIDR}" dev "${IFACE}"
ip link set "${IFACE}" up

echo "${IFACE} is up at ${HOST_CIDR}, owned by ${OWNER}."
