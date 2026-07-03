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
# Generates an HTML coverage report from `bazel coverage` output.
# Supports two coverage pipelines:
#   - LLVM (Linux): Custom reporter produces a zip with html_report/ inside.
#   - gcov (QNX):   Bazel's default reporter produces an LCOV text file;
#                   we convert it to HTML using lcov_to_html.py.
#
# Usage:
#   bazel run //quality/coverage:generate_coverage_html [-- [--archive <archive-name>] [--platform <platform>] [output-dir]]
#
# Arguments:
#   --archive <archive-name>  Also create a zip archive named <archive-name>.zip
#                             containing the HTML report, raw LCOV data and JUnit XMLs.
#   --platform <platform>     Target platform for justification filtering (linux or qnx).
#                             Default: linux. Also affects the default output directory
#                             (cpp_coverage for linux, cpp_coverage_qnx for qnx).
#   output-dir                Directory to write the HTML report to (default: cpp_coverage
#                             or cpp_coverage_<platform>)

set -euo pipefail

ARCHIVE_NAME=""
PLATFORM="linux"
OUTPUT_DIR=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --archive)
      ARCHIVE_NAME="${2:?--archive requires a name argument}"
      shift 2
      ;;
    --platform)
      PLATFORM="${2:?--platform requires a platform argument (linux or qnx)}"
      shift 2
      ;;
    *)
      OUTPUT_DIR="$1"
      shift
      ;;
  esac
done

# Set default output directory based on platform if not explicitly provided.
if [[ -z "${OUTPUT_DIR}" ]]; then
  if [[ "${PLATFORM}" == "linux" ]]; then
    OUTPUT_DIR="cpp_coverage"
  else
    OUTPUT_DIR="cpp_coverage_${PLATFORM}"
  fi
fi

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

# The coverage report is at _coverage_report.dat. Its format depends on the
# pipeline:
#   - LLVM (--config=llvm_cov): zip containing html_report/, lcov_report/, text_report/
#   - gcov (default/QNX):       plain LCOV text file
COVERAGE_REPORT="${BUILD_WORKSPACE_DIRECTORY}/bazel-out/_coverage/_coverage_report.dat"

if [[ ! -f "${COVERAGE_REPORT}" ]]; then
  echo "ERROR: Coverage report not found at ${COVERAGE_REPORT}" >&2
  echo "       Run 'bazel coverage //... --build_tests_only' first." >&2
  exit 1
fi

TMPDIR_EXTRACT="${TMPDIR:-/tmp}/coverage_extract_$$"
mkdir -p "${TMPDIR_EXTRACT}"
trap 'rm -rf "${TMPDIR_EXTRACT}"' EXIT

rm -rf "${OUTPUT_DIR}"

# Detect the report format: zip (LLVM pipeline) vs plain text (gcov pipeline).
if file -b "${COVERAGE_REPORT}" | grep -q "Zip archive"; then
  # -----------------------------------------------------------------------
  # LLVM pipeline: extract HTML from the zip produced by our custom reporter.
  # -----------------------------------------------------------------------
  unzip -q -o "${COVERAGE_REPORT}" -d "${TMPDIR_EXTRACT}"

  if [[ -d "${TMPDIR_EXTRACT}/html_report" ]]; then
    cp -r "${TMPDIR_EXTRACT}/html_report" "${OUTPUT_DIR}"
  else
    echo "ERROR: html_report/ not found in ${COVERAGE_REPORT}" >&2
    exit 1
  fi
else
  # -----------------------------------------------------------------------
  # gcov pipeline: _coverage_report.dat is an LCOV text file.
  # Convert to HTML using lcov_to_html (genhtml-compatible Python tool).
  # -----------------------------------------------------------------------

  # Copy the raw LCOV data for later archiving.
  cp "${COVERAGE_REPORT}" "${TMPDIR_EXTRACT}/lcov.dat"

  echo "Generating HTML report from LCOV data..."
  bazel run //quality/coverage/llvm_cov:lcov_to_html -- \
    --lcov "${TMPDIR_EXTRACT}/lcov.dat" \
    --output-dir "${OUTPUT_DIR}" \
    --source-root "${BUILD_WORKSPACE_DIRECTORY}" \
    --filter-regexes "${BUILD_WORKSPACE_DIRECTORY}/quality/coverage/llvm_cov/filter_regexes.txt"
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
      --platform "${PLATFORM}" \
      --output "${JUSTIFICATION_DIR}/manifest.json"; then

    # Run effective_coverage.py via Bazel to post-process HTML and calculate effective coverage.
    EFFECTIVE_COV_ARGS=(
        --html-dir "${OUTPUT_DIR}"
        --manifest "${JUSTIFICATION_DIR}/manifest.json"
        --output "${JUSTIFICATION_DIR}/report.json"
    )
    # For gcov pipeline, pass the LCOV data for reliable totals parsing.
    if [[ -f "${TMPDIR_EXTRACT}/lcov.dat" ]]; then
      EFFECTIVE_COV_ARGS+=(--lcov "${TMPDIR_EXTRACT}/lcov.dat")
    fi
    bazel run //quality/coverage/llvm_cov:effective_coverage -- "${EFFECTIVE_COV_ARGS[@]}"
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

  # Include the LCOV .dat file (from zip for LLVM, or directly for gcov).
  if [[ -f "${TMPDIR_EXTRACT}/lcov_report/lcov.dat" ]]; then
    cp "${TMPDIR_EXTRACT}/lcov_report/lcov.dat" artifacts/coverage_report.dat
  elif [[ -f "${TMPDIR_EXTRACT}/lcov.dat" ]]; then
    cp "${TMPDIR_EXTRACT}/lcov.dat" artifacts/coverage_report.dat
  fi

  zip -r "${ARCHIVE_NAME}.zip" artifacts/
  rm -rf artifacts/
  echo "Coverage archive written to: ${ARCHIVE_NAME}.zip"
fi
