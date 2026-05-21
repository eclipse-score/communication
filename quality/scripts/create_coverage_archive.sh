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
# Assembles and zips the coverage artifacts (HTML report + LCOV .dat + test XMLs).
#
# Usage:
#   bash quality/scripts/create_coverage_archive.sh <archive-name> [html-dir] [lcov.dat]
#
# Arguments:
#   archive-name  Base name for the output zip (without .zip extension, required)
#   html-dir      Directory containing the genhtml output (default: cpp_coverage)
#   lcov.dat      Path to the raw LCOV data file
#                 (default: <bazel output_path>/_coverage/_coverage_report.dat)

set -euo pipefail

ARCHIVE_NAME="${1:?Usage: $0 <archive-name> [html-dir] [lcov.dat]}"
COVERAGE_HTML_DIR="${2:-cpp_coverage}"
LCOV_DAT="${3:-$(bazel info output_path)/_coverage/_coverage_report.dat}"

mkdir -p artifacts

# Copy JUnit XML test results preserving directory structure
find bazel-testlogs/score/ -name 'test.xml' -print0 \
  | xargs -0 -I{} cp --parents {} artifacts/

# Copy the HTML coverage report
cp -r "${COVERAGE_HTML_DIR}" artifacts/

# Include the raw LCOV .dat so the quality dashboard can read
# line/function/branch percentages without re-running genhtml.
if [[ -f "${LCOV_DAT}" ]]; then
  cp "${LCOV_DAT}" artifacts/coverage_report.dat
fi

zip -r "${ARCHIVE_NAME}.zip" artifacts/
