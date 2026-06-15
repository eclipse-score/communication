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
# Extracts the HTML coverage report from the llvm-cov generated zip produced by
# `bazel coverage`. Optionally assembles and zips the report together with
# LCOV data and JUnit XMLs.
#
# Usage:
#   bazel run //quality/coverage:generate_coverage_html [-- [--archive <archive-name>] [output-dir]]
#
# Arguments:
#   --archive <archive-name>  Also create a zip archive named <archive-name>.zip
#                             containing the HTML report, raw LCOV data and JUnit XMLs.
#   output-dir                Directory to write the HTML report to (default: cpp_coverage)

set -euo pipefail

ARCHIVE_NAME=""
OUTPUT_DIR="cpp_coverage"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --archive)
      ARCHIVE_NAME="${2:?--archive requires a name argument}"
      shift 2
      ;;
    *)
      OUTPUT_DIR="$1"
      shift
      ;;
  esac
done

# Change to the workspace root first so that all subsequent bazel calls
# and relative paths work correctly.
#
# IMPORTANT: bootstrap RUNFILES_DIR from $0 BEFORE the cd, since after cd
# any relative paths would break.  We ensure it is absolute here.
# Do NOT use 'readlink -f $0' — that resolves the sh_binary symlink to the
# source .sh file, losing the adjacent .runfiles tree.
_SELF_DIR="$(cd "$(dirname "$0")" && pwd)"
_SELF_NAME="$(basename "$0")"
if [[ -z "${RUNFILES_DIR:-}" || ! -d "${RUNFILES_DIR}" ]]; then
  if [[ -d "${_SELF_DIR}/${_SELF_NAME}.runfiles" ]]; then
    export RUNFILES_DIR="${_SELF_DIR}/${_SELF_NAME}.runfiles"
  fi
fi
unset _SELF_DIR _SELF_NAME

cd "${BUILD_WORKSPACE_DIRECTORY}"

# Resolve OUTPUT_DIR to absolute path (relative to workspace root).
OUTPUT_DIR="${BUILD_WORKSPACE_DIRECTORY}/${OUTPUT_DIR}"

# The coverage report generator produces a zip file at _coverage_report.dat
# containing: html_report/, lcov_report/lcov.dat, text_report/summary.txt
COVERAGE_ZIP="${BUILD_WORKSPACE_DIRECTORY}/bazel-out/_coverage/_coverage_report.dat"

if [[ ! -f "${COVERAGE_ZIP}" ]]; then
  echo "ERROR: Coverage report not found at ${COVERAGE_ZIP}" >&2
  echo "       Run 'bazel coverage //... --build_tests_only' first." >&2
  exit 1
fi

# Extract the HTML report from the zip.
TMPDIR_EXTRACT="${TMPDIR:-/tmp}/coverage_extract_$$"
mkdir -p "${TMPDIR_EXTRACT}"
trap 'rm -rf "${TMPDIR_EXTRACT}"' EXIT

unzip -q -o "${COVERAGE_ZIP}" -d "${TMPDIR_EXTRACT}"

# Copy the HTML report to the output directory.
rm -rf "${OUTPUT_DIR}"
if [[ -d "${TMPDIR_EXTRACT}/html_report" ]]; then
  cp -r "${TMPDIR_EXTRACT}/html_report" "${OUTPUT_DIR}"
else
  echo "ERROR: html_report/ not found in ${COVERAGE_ZIP}" >&2
  exit 1
fi

echo "Coverage report written to: ${OUTPUT_DIR}"

# ---------------------------------------------------------------------------
# Run coverage justification processing.
# ---------------------------------------------------------------------------
JUSTIFICATION_YAML="${BUILD_WORKSPACE_DIRECTORY}/quality/coverage/coverage_justifications.yaml"

if [[ -f "${JUSTIFICATION_YAML}" ]]; then
  echo ""
  echo "Running coverage justification processing..."

  JUSTIFICATION_DIR="${TMPDIR_EXTRACT}/justification_report"
  mkdir -p "${JUSTIFICATION_DIR}"

  # Run justify.py via Bazel to produce the resolved manifest.
  if bazel run //quality/coverage/llvm_cov:justify -- \
      --yaml "${JUSTIFICATION_YAML}" \
      --source-root "${BUILD_WORKSPACE_DIRECTORY}" \
      --output "${JUSTIFICATION_DIR}/manifest.json"; then

    # Run effective_coverage.py via Bazel to post-process HTML and calculate effective coverage.
    bazel run //quality/coverage/llvm_cov:effective_coverage -- \
        --html-dir "${OUTPUT_DIR}" \
        --manifest "${JUSTIFICATION_DIR}/manifest.json" \
        --output "${JUSTIFICATION_DIR}/report.json"
  fi

  # Display effective coverage summary.
  if [[ -f "${JUSTIFICATION_DIR}/summary.txt" ]]; then
    echo ""
    cat "${JUSTIFICATION_DIR}/summary.txt"

    # Extract effective coverage percentage for threshold check.
    EFFECTIVE_PCT=$(grep -oP 'Effective line coverage:\s+\K[0-9.]+' \
      "${JUSTIFICATION_DIR}/summary.txt" 2>/dev/null || echo "0")

    # Threshold check (default: 100%)
    THRESHOLD="${COVERAGE_THRESHOLD:-100}"
    if awk "BEGIN {exit (${EFFECTIVE_PCT} >= ${THRESHOLD}) ? 0 : 1}"; then
      :
    else
      echo "WARNING: Effective coverage ${EFFECTIVE_PCT}% is below threshold ${THRESHOLD}%" >&2
    fi
  fi
else
  echo "INFO: No coverage_justifications.yaml found, skipping justification processing."
fi

# ---------------------------------------------------------------------------
# Optional: create a zip archive with the HTML report, raw LCOV data and
# JUnit XML test results.
# ---------------------------------------------------------------------------
if [[ -n "${ARCHIVE_NAME}" ]]; then
  mkdir -p artifacts

  # Copy JUnit XML test results preserving directory structure
  find bazel-testlogs/score/ -name 'test.xml' -exec cp --parents {} artifacts/ \;

  # Copy the HTML coverage report
  cp -r "${OUTPUT_DIR}" artifacts/

  # Include the LCOV .dat file from the zip (for backward compat with dashboards).
  if [[ -f "${TMPDIR_EXTRACT}/lcov_report/lcov.dat" ]]; then
    cp "${TMPDIR_EXTRACT}/lcov_report/lcov.dat" artifacts/coverage_report.dat
  fi

  zip -r "${ARCHIVE_NAME}.zip" artifacts/
  rm -rf artifacts/
  echo "Coverage archive written to: ${ARCHIVE_NAME}.zip"
fi
