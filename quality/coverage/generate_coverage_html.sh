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
# Optionally assembles and zips the report together with LCOV data and JUnit XMLs.
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

# bazel-out/ is a symlink Bazel always creates in the workspace root, it
# points to the real output directory.  _coverage/ sits at its root (not
# inside a config-specific sub-directory), so we can locate the merged
# report without calling 'bazel info' — which would require 'bazel' to be
# on PATH inside the run environment.
LCOV_DAT="${BUILD_WORKSPACE_DIRECTORY}/bazel-out/_coverage/_coverage_report.dat"

# ---------------------------------------------------------------------------
# Resolve genhtml: prefer Bazel-managed binary from @lcov_deb runfiles so
# that no system lcov installation is required.  Fall back to PATH.
#
# Bazel uses a symlink forest on Linux: runfiles entries are symlinks to the
# real cached files.  Use 'find -L' to dereference them ('find -type f'
# without -L never matches symlinks).
# ---------------------------------------------------------------------------
_tool_path() {
  local name="$1"
  local found=""
  # 1. Symlink forest (always present on Linux under bazel run)
  if [[ -n "${RUNFILES_DIR:-}" ]]; then
    found=$(find -L "${RUNFILES_DIR}" -path "*lcov_deb/usr/bin/${name}" -type f 2>/dev/null | head -1)
  fi
  # 2. PATH
  if [[ -z "${found}" ]]; then
    found=$(command -v "${name}" 2>/dev/null || true)
  fi
  echo "${found}"
}

GENHTML="$(_tool_path genhtml)"
LCOV="$(_tool_path lcov)"

if [[ -z "$GENHTML" ]]; then
  echo "ERROR: 'genhtml' not found. Run via 'bazel run //quality/coverage:generate_coverage_html' or install 'lcov'." >&2
  exit 1
fi
if [[ -z "$LCOV" ]]; then
  echo "ERROR: 'lcov' not found. Run via 'bazel run //quality/coverage:generate_coverage_html' or install 'lcov'." >&2
  exit 1
fi

# When using the Bazel-managed tool, set PERL5LIB so Perl finds lcovutil.pm.
# lcovutil.pm lives two levels above the genhtml binary: bin/ → usr/ → lib/lcov.
lcov_lib="$(dirname "$(dirname "${GENHTML}")")/lib/lcov"
if [[ -d "${lcov_lib}" ]]; then
  export PERL5LIB="${lcov_lib}${PERL5LIB:+:${PERL5LIB}}"
fi

# ---------------------------------------------------------------------------
# Filter source files from LCOV data before generating HTML.
# The --instrumentation_filter in coverage.bazelrc already excludes external
# deps (third_party, gtest) and test/ subdirectories at compile time.
# Only files that slip through because they live in mixed packages need
# removal here:
#   - *mock*.h/cpp      mock headers in production packages (e.g. configuration/)
# ---------------------------------------------------------------------------
LCOV_DAT_FILTERED="${TMPDIR:-/tmp}/coverage_report_filtered_$$.dat"
"${LCOV}" --remove "${LCOV_DAT}" \
  '*mock*.h' \
  '*mock*.cpp' \
  --output-file "${LCOV_DAT_FILTERED}" \
  --ignore-errors unused

# NOTE: "--ignore-errors category,inconsistent"
# LLVM coverage writes per-process .profraw files that are merged during
# bazel's post-processing step.  The merge can occasionally leave
# inconsistent hit counts that genhtml rejects.  This flag tells genhtml to
# silently skip those entries instead of aborting, coverage numbers are
# slightly under-counted for affected translation units but the report still
# generates.
"${GENHTML}" "${LCOV_DAT_FILTERED}" \
  --output-directory "${OUTPUT_DIR}" \
  --show-details \
  --legend \
  --function-coverage \
  --branch-coverage \
  --rc no_exception_branch=1 \
  --ignore-errors category,inconsistent

echo "Coverage report written to: ${OUTPUT_DIR}"

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

  # Include the raw LCOV .dat so the quality dashboard can read
  # line/function/branch percentages without re-running genhtml.
  if [[ -f "${LCOV_DAT}" ]]; then
    cp "${LCOV_DAT}" artifacts/coverage_report.dat
  fi

  zip -r "${ARCHIVE_NAME}.zip" artifacts/
  rm -rf artifacts/
  echo "Coverage archive written to: ${ARCHIVE_NAME}.zip"
fi
