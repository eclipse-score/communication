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
"""Generates the golden file of public targets."""

import os
import sys

from quality.visibility_guard.parser import get_all_public_targets


def main():
    # BUILD_WORKSPACE_DIRECTORY is set by `bazel run`
    workspace_dir = os.environ.get("BUILD_WORKSPACE_DIRECTORY", "")
    if workspace_dir and os.path.exists(os.path.join(workspace_dir, "MODULE.bazel")):
        repo_root = workspace_dir
    else:
        # Fallback: navigate up from real script location
        repo_root = os.path.dirname(os.path.realpath(__file__))
        while repo_root != "/" and not os.path.exists(
            os.path.join(repo_root, "MODULE.bazel")
        ):
            repo_root = os.path.dirname(repo_root)

    if not os.path.exists(os.path.join(repo_root, "MODULE.bazel")):
        print("ERROR: Could not find repository root (MODULE.bazel)")
        sys.exit(1)

    public_targets = get_all_public_targets(repo_root)

    golden_path = os.path.join(
        repo_root, "quality", "visibility_guard", "public_targets.golden"
    )
    with open(golden_path, "w") as f:
        f.write("\n".join(public_targets) + "\n")

    print(f"Written {len(public_targets)} public targets to {golden_path}")


if __name__ == "__main__":
    main()
