#!/usr/bin/env bash

# *******************************************************************************
# Copyright (c) 2025 Contributors to the Eclipse Foundation
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

# --- begin runfiles.bash initialization ---
# Copy-pasted from Bazel's Bash runfiles library (tools/bash/runfiles/runfiles.bash).
#set -euo pipefail
if [[ ! -d "${RUNFILES_DIR:-/dev/null}" && ! -f "${RUNFILES_MANIFEST_FILE:-/dev/null}" ]]; then
  if [[ -f "$0.runfiles_manifest" ]]; then
    export RUNFILES_MANIFEST_FILE="$0.runfiles_manifest"
  elif [[ -f "$0.runfiles/MANIFEST" ]]; then
    export RUNFILES_MANIFEST_FILE="$0.runfiles/MANIFEST"
  elif [[ -f "$0.runfiles/bazel_tools/tools/bash/runfiles/runfiles.bash" ]]; then
    export RUNFILES_DIR="$0.runfiles"
  fi
fi
if [[ -f "${RUNFILES_DIR:-/dev/null}/bazel_tools/tools/bash/runfiles/runfiles.bash" ]]; then
  source "${RUNFILES_DIR}/bazel_tools/tools/bash/runfiles/runfiles.bash"
elif [[ -f "${RUNFILES_MANIFEST_FILE:-/dev/null}" ]]; then
  source "$(grep -m1 "^bazel_tools/tools/bash/runfiles/runfiles.bash " \
            "$RUNFILES_MANIFEST_FILE" | cut -d ' ' -f 2-)"
else
  echo >&2 "ERROR: cannot find @bazel_tools//tools/bash/runfiles:runfiles.bash"
  exit 1
fi
# --- end runfiles.bash initialization ---

rm -rf test_workspace
mkdir -p test_workspace
touch test_workspace/WORKSPACE

# Unit under test
ln -sf "$(rlocation $TEST_WORKSPACE/quality/sanitizer/sanitizer.bazelrc)" "test_workspace/.bazelrc"

# Test-Workspace
ln -sf "$(rlocation $TEST_WORKSPACE/quality/sanitizer/test_workspace/MODULE.bazel)" "test_workspace/MODULE.bazel"
ln -sf "$(rlocation $TEST_WORKSPACE/quality/sanitizer/test_workspace/BUILD.tpl)" "test_workspace/BUILD"
ln -sf "$(rlocation $TEST_WORKSPACE/quality/sanitizer/test_workspace/asan_fail_heap_out_of_bounds.cpp)" "test_workspace/asan_fail_heap_out_of_bounds.cpp"
ln -sf "$(rlocation $TEST_WORKSPACE/quality/sanitizer/test_workspace/tsan_fail_data_race.cpp)" "test_workspace/tsan_fail_data_race.cpp"

cd test_workspace

if bazel test --config=asan //...; then
  exit 1;
fi

if bazel test --config=tsan //...; then
  exit 1;
fi

exit 0;
