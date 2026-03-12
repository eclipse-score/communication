#!/usr/bin/env python3

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

"""
find_dependencies.py
Parses a PR description and extracts Depends-On: references.

Reads EVENT_NAME, PR_NUMBER, GH_TOKEN from environment variables.
Calls 'gh pr view' to fetch the PR body.
Writes dependency_prs= and dependency_count= directly to $GITHUB_OUTPUT.
"""

import os
import re
import subprocess
import sys


def parse_depends_on(pr_body: str) -> list[str]:
    """Parse Depends-On patterns from PR body text.

    Supports formats: "Depends-On: #123", "Depends-On: 123", "Depends-On:#123"
    Case insensitive. Returns a list of PR number strings.
    """
    if not pr_body:
        return []
    return re.findall(r"(?i)depends-on:\s*#?(\d+)", pr_body)


def get_pr_body(pr_number: str) -> str:
    """Fetch the PR body text using the GitHub CLI.

    Raises:
        RuntimeError: if the gh CLI exits with a non-zero return code.
    """
    result = subprocess.run(
        ["gh", "pr", "view", pr_number, "--json", "body", "--jq", ".body"],
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        raise RuntimeError(
            f"gh CLI failed for PR #{pr_number} (exit {result.returncode}): {result.stderr.strip()}"
        )
    return result.stdout.strip()


def write_github_output(output_path: str, prs: list[str]) -> None:
    """Write dependency_prs and dependency_count to the GITHUB_OUTPUT file."""
    with open(output_path, "a", encoding="utf-8") as f:
        f.write(f"dependency_prs={','.join(prs)}\n")
        f.write(f"dependency_count={len(prs)}\n")


def main() -> int:
    event_name = os.environ.get("EVENT_NAME", "")
    pr_number = os.environ.get("PR_NUMBER", "")
    output_path = os.environ.get("GITHUB_OUTPUT", os.devnull)

    if event_name not in ("pull_request", "pull_request_target", "workflow_run"):
        print("Not a pull request event, skipping dependency check")
        write_github_output(output_path, [])
        return 0

    if not pr_number:
        print("Could not determine PR number")
        write_github_output(output_path, [])
        return 0

    print(f"Checking PR #{pr_number} for dependencies...")

    try:
        pr_body = get_pr_body(pr_number)
    except RuntimeError as exc:
        print(f"::error::{exc}")
        write_github_output(output_path, [])
        return 1

    if not pr_body:
        print("Could not fetch PR description or description is empty")
        write_github_output(output_path, [])
        return 0

    dependency_prs = parse_depends_on(pr_body)

    if not dependency_prs:
        print("No 'Depends-On:' keyword found in PR description")
        write_github_output(output_path, [])
        return 0

    print(f"Found {len(dependency_prs)} dependency PR(s): {','.join(dependency_prs)}")
    write_github_output(output_path, dependency_prs)
    return 0


if __name__ == "__main__":
    sys.exit(main())

