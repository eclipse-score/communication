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
# Generates an HTML coverage report from an LCOV .dat file using genhtml.
#
# Usage:
#   bash quality/scripts/generate_coverage_html.sh <lcov.dat> [output-dir]
#
# Arguments:
#   lcov.dat    Path to the LCOV .dat coverage data file (required)
#   output-dir  Directory to write the HTML report to (default: cpp_coverage)

set -euo pipefail

LCOV_DAT="${1:?Usage: $0 <lcov.dat> [output-dir]}"
OUTPUT_DIR="${2:-cpp_coverage}"

# NOTE: "--ignore-errors category,inconsistent"
# Root cause: when Bazel runs tests in parallel, multiple test processes can
# write to the same .gcda counter files concurrently. The overlapping writes
# corrupt hit counts; genhtml then rejects the file with an "inconsistent"
# category error and aborts.
#
# This flag tells genhtml to silently skip corrupted entries instead of
# aborting. Coverage numbers may be slightly under-counted for translation
# units affected by a race, but the report still generates.
#
# Known alternatives (not yet adopted):
#   A) Add --local_test_jobs=1 to the bazel coverage command.
#      Tests execute sequentially → no races, exact counts.
#      Trade-off: longer nightly runtime (build parallelism is unaffected;
#      only test execution is serialised).
#   B) Switch to LLVM coverage (bazel coverage --experimental_generate_llvm_lcov).
#      llvm-cov writes per-process .profraw files → no shared-file races by
#      design, accurate counts, no slowdown.
#      Trade-off: requires building with Clang — a broader toolchain change.
genhtml "${LCOV_DAT}" \
  --output-directory "${OUTPUT_DIR}" \
  --show-details \
  --legend \
  --function-coverage \
  --branch-coverage \
  --ignore-errors category,inconsistent
