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
#   bazel run //quality/coverage:generate_coverage_html [-- [output-dir]]
#
# Arguments:
#   output-dir  Directory to write the HTML report to (default: cpp_coverage)

set -euo pipefail

LCOV_DAT="$(bazel info output_path)/_coverage/_coverage_report.dat"
OUTPUT_DIR="${1:-$(mktemp -d)}"

# ---------------------------------------------------------------------------
# Resolve genhtml: prefer Bazel-managed binary from @lcov_deb runfiles so
# that no system lcov installation is required.  Fall back to PATH.
# ---------------------------------------------------------------------------
_tool_path() {
  local name="$1"
  local found=""
  if [[ -n "${RUNFILES_DIR:-}" ]]; then
    found=$(find "${RUNFILES_DIR}" -path "*/lcov_deb/usr/bin/${name}" -type f 2>/dev/null | head -1)
  fi
  if [[ -z "${found}" ]]; then
    found=$(command -v "${name}" 2>/dev/null || true)
  fi
  echo "${found}"
}

GENHTML="$(_tool_path genhtml)"

if [[ -z "$GENHTML" ]]; then
  echo "ERROR: 'genhtml' not found. Run via 'bazel run //quality/coverage:generate_coverage_html' or install 'lcov'." >&2
  exit 1
fi

# When using the Bazel-managed tool, set PERL5LIB so Perl finds lcovutil.pm.
if [[ -n "${RUNFILES_DIR:-}" ]]; then
  lcov_lib=$(find "${RUNFILES_DIR}" -path "*/lcov_deb/usr/lib/lcov" -type d 2>/dev/null | head -1)
  if [[ -n "${lcov_lib}" ]]; then
    export PERL5LIB="${lcov_lib}${PERL5LIB:+:${PERL5LIB}}"
  fi
fi
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
"${GENHTML}" "${LCOV_DAT}" \
  --output-directory "${OUTPUT_DIR}" \
  --show-details \
  --legend \
  --function-coverage \
  --branch-coverage \
  --ignore-errors category,inconsistent

echo "Coverage report written to: ${OUTPUT_DIR}"
