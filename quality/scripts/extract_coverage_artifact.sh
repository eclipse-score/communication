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
# Extracts the coverage zip artifact and copies HTML + LCOV data to the
# quality output directory for the dashboard generator.
#
# Usage:
#   bash quality/scripts/extract_coverage_artifact.sh [zip-dir] [output-dir]
#
# Arguments:
#   zip-dir     Directory containing the downloaded coverage zip (default: /tmp/coverage_zip)
#   output-dir  Directory to copy the HTML report into (default: ${GITHUB_WORKSPACE}/_quality/coverage)

set -euo pipefail

ZIP_DIR="${1:-/tmp/coverage_zip}"
OUTPUT_DIR="${2:-${GITHUB_WORKSPACE}/_quality/coverage}"

cd "${ZIP_DIR}"
unzip *.zip -d extracted

mkdir -p "${OUTPUT_DIR}"
cp -r extracted/artifacts/cpp_coverage_linux/. "${OUTPUT_DIR}/"
