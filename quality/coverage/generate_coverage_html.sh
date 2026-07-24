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
#                             (cpp_coverage_linux for linux, cpp_coverage_qnx for qnx).
#   output-dir                Directory to write the HTML report to (default: cpp_coverage_<platform>)

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
    OUTPUT_DIR="cpp_coverage_${PLATFORM}"
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

  echo "Building collected coverage allowlist for QNX..."
  # coverage_scope depends on Linux-only dependable element indices.
  # Build it in the host/default config and reuse the workspace-relative allowlist for QNX filtering.
  bazel build --config=qnx //quality/coverage:coverage_scope --output_groups=allowlist
  ALLOWLIST_FILE="$(bazel info bazel-bin)/quality/coverage/coverage_scope_allowlist.txt"
  if [[ ! -f "${ALLOWLIST_FILE}" ]]; then
    echo "ERROR: Allowlist file not found at ${ALLOWLIST_FILE}" >&2
    exit 1
  fi

  # Collect zero-coverage baseline LCOV records so that files not exercised by
  # any test still appear in the HTML report with 0% coverage, rather than
  # being silently omitted. Two sources contribute: (a) baseline_coverage.dat
  # files placed alongside each coverage.dat by the Bazel coverage runner, and
  # (b) _cc_coverage.dat files produced by dedicated baseline_coverage_test
  # targets for C++ translation units that the GCOV runner handles separately.
  BASELINE_LCOV="${TMPDIR_EXTRACT}/baseline_lcov.dat"
  BASELINE_COUNT=0
  LCOV_INPUT_LIST="${BUILD_WORKSPACE_DIRECTORY}/bazel-out/_coverage/lcov_files.tmp"
  if [[ -f "${LCOV_INPUT_LIST}" ]]; then
    # Collect baseline coverage next to each coverage.dat listed by Bazel.
    # Some runners list baseline_coverage.dat directly, others only coverage.dat.
    while IFS= read -r lcov_input; do
      if [[ "${lcov_input}" == *"/baseline_coverage.dat" ]]; then
        if [[ -f "${lcov_input}" ]]; then
          printf "\n" >> "${BASELINE_LCOV}"
          cat "${lcov_input}" >> "${BASELINE_LCOV}"
          printf "\n" >> "${BASELINE_LCOV}"
          BASELINE_COUNT=$((BASELINE_COUNT + 1))
        fi
        continue
      fi
      if [[ "${lcov_input}" == *"/coverage.dat" ]]; then
        baseline_input="${lcov_input%coverage.dat}baseline_coverage.dat"
        if [[ -f "${baseline_input}" ]]; then
          printf "\n" >> "${BASELINE_LCOV}"
          cat "${baseline_input}" >> "${BASELINE_LCOV}"
          printf "\n" >> "${BASELINE_LCOV}"
          BASELINE_COUNT=$((BASELINE_COUNT + 1))
        fi
      fi
    done < <(sort -u "${LCOV_INPUT_LIST}")
  fi

  # Some baseline records with full DA line data are emitted through dedicated
  # baseline_coverage_test targets and only exist as _cc_coverage.dat files.
  # Merge them as additional baseline input.
  while IFS= read -r cc_baseline; do
    printf "\n" >> "${BASELINE_LCOV}"
    cat "${cc_baseline}" >> "${BASELINE_LCOV}"
    printf "\n" >> "${BASELINE_LCOV}"
    BASELINE_COUNT=$((BASELINE_COUNT + 1))
  done < <(find -L "${BUILD_WORKSPACE_DIRECTORY}/bazel-out" \
    -path "*/testlogs/*/baseline_coverage_test/_coverage/_cc_coverage.dat" \
    -type f | sort -u)

  echo "Generating HTML report from LCOV data..."
  LCOV_TO_HTML_ARGS=(
    --lcov "${TMPDIR_EXTRACT}/lcov.dat"
    --output-dir "${OUTPUT_DIR}"
    --source-root "${BUILD_WORKSPACE_DIRECTORY}"
    --allowlist "${ALLOWLIST_FILE}"
  )

  if [[ -s "${BASELINE_LCOV}" ]]; then
    echo "Merging ${BASELINE_COUNT} baseline_coverage.dat files into report input..."
    LCOV_TO_HTML_ARGS+=(--baseline-lcov "${BASELINE_LCOV}")
  fi

  bazel run //quality/coverage:lcov_to_html -- "${LCOV_TO_HTML_ARGS[@]}"
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
  if bazel run //quality/coverage:justify -- \
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
    bazel run //quality/coverage:effective_coverage -- "${EFFECTIVE_COV_ARGS[@]}"
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
