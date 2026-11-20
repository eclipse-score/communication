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

# Removes the tap device setup_tap.sh creates; it otherwise lives until the host reboots.
#   bazel run //score/lg-someip/tests/benchmark/test_cases:teardown_tap
set -euo pipefail

IFACE="tap0"

if [[ $(id -u) -ne 0 ]]; then
    echo "This needs root to remove a tap device; re-running under sudo." >&2
    exec sudo -- "$0" "$@"
fi

if ! ip link show "${IFACE}" >/dev/null 2>&1; then
    echo "${IFACE} does not exist; nothing to remove."
    exit 0
fi

ip link delete "${IFACE}"
echo "Removed ${IFACE}."
