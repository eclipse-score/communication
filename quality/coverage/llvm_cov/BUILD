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

load("@rules_python//python:defs.bzl", "py_binary")
load("@rules_shell//shell:sh_binary.bzl", "sh_binary")

py_binary(
    name = "merger",
    srcs = ["merger.py"],
)

py_binary(
    name = "reporter",
    srcs = ["reporter.py"],
    data = [
        "filter_regexes.txt",
        "//:MODULE.bazel",
        "@llvm_toolchain//:llvm-cov",
        "@llvm_toolchain//:llvm-profdata",
    ],
    deps = ["@rules_python//python/runfiles"],
)

genrule(
    name = "reporter_wrapper_gen",
    srcs = [
        "filter_regexes.txt",
        "//:MODULE.bazel",
    ],
    outs = ["reporter_wrapper.sh"],
    cmd = """
cat > $@ << EOF
#!/usr/bin/env bash
set -euo pipefail
if [[ -z "\\$${RUNFILES_DIR:-}" ]]; then
  if [[ -d "\\$$0.runfiles" ]]; then
    export RUNFILES_DIR="\\$$0.runfiles"
  fi
fi
WORKSPACE_ROOT="\\$$(cd "\\$$(dirname "\\$$(readlink -f "\\$${RUNFILES_DIR}/$(rlocationpath //:MODULE.bazel)")")" && pwd)/"
exec "\\$${RUNFILES_DIR}/_main/quality/coverage/llvm_cov/reporter" \\\\
  --filter_regexes="$(rlocationpath filter_regexes.txt)" \\\\
  --workspace_root="\\$${WORKSPACE_ROOT}" \\\\
  "\\$$@"
EOF
chmod +x $@
""",
)

sh_binary(
    name = "reporter_wrapper",
    srcs = [":reporter_wrapper_gen"],
    data = [":reporter"],
)

py_binary(
    name = "justify",
    srcs = ["justify.py"],
    deps = [
        "@score_communication_pip//pyyaml",
    ],
)

py_binary(
    name = "effective_coverage",
    srcs = ["effective_coverage.py"],
)
