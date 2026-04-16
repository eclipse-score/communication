#!/usr/bin/env bash

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

set -euo pipefail

BINARY="$1"
EXPECTED_EXIT_CODE="${2:-55}"

echo "Running: $BINARY"
echo "Expected exit code: $EXPECTED_EXIT_CODE"

# Run the binary and capture exit code
set +e
"$BINARY"
ACTUAL_EXIT_CODE=$?
set -e

echo "Actual exit code: $ACTUAL_EXIT_CODE"

# Verify the binary failed with the expected exit code
if [ "$ACTUAL_EXIT_CODE" -eq "$EXPECTED_EXIT_CODE" ]; then
    echo "✓ PASS: Binary failed with exit code $EXPECTED_EXIT_CODE (sanitizer detected violation)"
    exit 0
else
    echo "✗ FAIL: Binary exited with code $ACTUAL_EXIT_CODE, expected $EXPECTED_EXIT_CODE"
    echo "This means the sanitizer did NOT catch the expected violation!"
    exit 1
fi
