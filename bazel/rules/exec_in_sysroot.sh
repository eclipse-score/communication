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
# Generic wrapper script that executes a command in fakechroot using the provided sysroot.
#
# This wrapper is intentionally strict: fakechroot must come from the Bazel-
# provided sysroot so execution stays hermetic.
#
# Usage:
#   fakechroot_wrapper.sh <command-in-sysroot> [arguments...]
#
# Arguments:
#   command-in-sysroot  - Path to the command inside the sysroot (e.g., /usr/bin/lcov)
#   arguments...        - Arguments to pass to the command
#
# Environment variables:
#   SYSROOT_DIR           - Path to the sysroot directory (required)
#   BUILD_WORKSPACE_DIRECTORY - Automatically excluded from fakechroot (optional)
#   FAKECHROOT_EXCLUDE_PATH - Colon-separated paths to exclude from fakechroot (optional)

set -euo pipefail

COMMAND_IN_SYSROOT="${1:?Command in sysroot must be provided as first argument}"
shift || true

SYSROOT_DIR="${SYSROOT_DIR:?SYSROOT_DIR must be set}"
FAKECHROOT_BIN="${SYSROOT_DIR}/usr/bin/fakechroot"

if [[ ! -x "${FAKECHROOT_BIN}" ]]; then
  echo "ERROR: fakechroot not found in sysroot: ${FAKECHROOT_BIN}" >&2
  exit 1
fi

ARCH=""
case "$(uname -m)" in
  x86_64) ARCH="x86_64" ;;
  aarch64) ARCH="aarch64" ;;
esac

if [[ -z "${ARCH}" ]]; then
  echo "ERROR: unsupported host architecture: $(uname -m)" >&2
  exit 1
fi

FAKECHROOT_LIB=""
if [[ -n "${ARCH}" && -f "${SYSROOT_DIR}/usr/lib/${ARCH}-linux-gnu/fakechroot/libfakechroot.so" ]]; then
  FAKECHROOT_LIB="${SYSROOT_DIR}/usr/lib/${ARCH}-linux-gnu/fakechroot/libfakechroot.so"
fi
if [[ -z "${FAKECHROOT_LIB}" ]]; then
  FAKECHROOT_LIB="$(find "${SYSROOT_DIR}/usr/lib" -path '*/fakechroot/libfakechroot.so' -type f | head -1 || true)"
fi
if [[ -z "${FAKECHROOT_LIB}" ]]; then
  echo "ERROR: libfakechroot.so not found in sysroot" >&2
  exit 1
fi

# Build the exclude paths list.
# Always exclude BUILD_WORKSPACE_DIRECTORY if it's set, so workspace operations
# (reading data, writing reports) remain on the host filesystem.
EXCLUDE_PATHS=""
if [[ -n "${BUILD_WORKSPACE_DIRECTORY:-}" ]]; then
  EXCLUDE_PATHS="${BUILD_WORKSPACE_DIRECTORY}"
fi

# Append any user-specified exclude paths.
if [[ -n "${FAKECHROOT_EXCLUDE_PATH:-}" ]]; then
  if [[ -n "${EXCLUDE_PATHS}" ]]; then
    EXCLUDE_PATHS="${EXCLUDE_PATHS}:${FAKECHROOT_EXCLUDE_PATH}"
  else
    EXCLUDE_PATHS="${FAKECHROOT_EXCLUDE_PATH}"
  fi
fi

if [[ -n "${EXCLUDE_PATHS}" ]]; then
  export FAKECHROOT_EXCLUDE_PATH="${EXCLUDE_PATHS}"
fi

# Ensure fakechroot can resolve its preload library from the sysroot.
export LD_LIBRARY_PATH="${SYSROOT_DIR}/lib:${SYSROOT_DIR}/usr/lib:${SYSROOT_DIR}/usr/lib/x86_64-linux-gnu:${SYSROOT_DIR}/usr/lib/aarch64-linux-gnu:${SYSROOT_DIR}/usr/lib/x86_64-linux-gnu/fakechroot:${SYSROOT_DIR}/usr/lib/aarch64-linux-gnu/fakechroot:${LD_LIBRARY_PATH:-}"

exec "${FAKECHROOT_BIN}" --lib "${FAKECHROOT_LIB}" -- chroot "${SYSROOT_DIR}" "${COMMAND_IN_SYSROOT}" "$@"
