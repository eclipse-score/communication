#!/usr/bin/env bash
# *******************************************************************************
# Copyright (C) 2017-2026 LG Electronics Inc.
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

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPORT_DIR="${REPORT_DIR:-${ROOT_DIR}/tests/test_report}"
BUILD_DIR="${1:-${ROOT_DIR}/build-tests}"

if [[ -d "${BUILD_DIR}" ]]; then
	find "${BUILD_DIR}" -type f -name '*.gcda' -delete
fi
cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" \
	-DENABLE_TEST_BUILD=ON \
	-DBUILD_TESTING=ON
cmake --build "${BUILD_DIR}" --parallel
ctest --test-dir "${BUILD_DIR}" --output-on-failure

if command -v lcov >/dev/null 2>&1 && command -v genhtml >/dev/null 2>&1; then
	mkdir -p "${REPORT_DIR}"
	RAW_TRACEFILE="${REPORT_DIR}/output_lcov.raw"
	TRACEFILE="${REPORT_DIR}/output_lcov"
	LCOV_CAPTURE_LOG="${REPORT_DIR}/lcov_capture.log"
	lcov -o "${RAW_TRACEFILE}" -c --no-external -d "${BUILD_DIR}" \
		--base-directory "${ROOT_DIR}" \
		--ignore-errors mismatch,unused 2>"${LCOV_CAPTURE_LOG}"
	lcov --remove "${RAW_TRACEFILE}" \
		"${ROOT_DIR}/tests/*" \
		"${ROOT_DIR}/third_party/*" \
		"${ROOT_DIR}/tools/*" \
		"${ROOT_DIR}/examples/*" \
		'*/_deps/*' \
		'/usr/*' \
		-o "${TRACEFILE}" \
		--ignore-errors mismatch,unused
	genhtml -o "${REPORT_DIR}/report" "${TRACEFILE}" --ignore-errors mismatch,unused
else
	echo "lcov/genhtml not found; skipping coverage report generation." >&2
fi