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
check_dependencies_merged.py
Self-contained script that checks whether all 'Depends-On:' dependency PRs
referenced in a PR description are already merged.

Reads EVENT_NAME, PR_NUMBER, GH_TOKEN from environment variables.
Calls 'gh pr view' to fetch PR data.
Writes dependency_prs=, dependency_count=, all_dependencies_merged=, and
unmerged_prs= directly to $GITHUB_OUTPUT.

Exit codes:
    0 – no dependencies, or all dependencies are merged
    1 – at least one dependency PR is not yet merged (or an error occurred)
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
            f"gh CLI failed for PR #{pr_number} (exit {result.returncode}): "
            f"{result.stderr.strip()}"
        )
    return result.stdout.strip()


def get_pr_state(pr_number: str) -> str:
    """Get the state of a PR using the GitHub CLI.

    Returns one of "OPEN", "CLOSED", "MERGED", or "" on error.
    """
    result = subprocess.run(
        ["gh", "pr", "view", pr_number, "--json", "state", "--jq", ".state"],
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        return ""
    return result.stdout.strip()


def check_dependencies(pr_numbers: list[str]) -> tuple[bool, list[str]]:
    """Check whether all dependency PRs are merged.

    Returns:
        A tuple (all_merged, unmerged_prs) where all_merged is True only if
        every dependency PR has state MERGED, and unmerged_prs lists the PR
        numbers that are not yet merged.
    """
    unmerged: list[str] = []

    for pr_number in pr_numbers:
        state = get_pr_state(pr_number)
        print(f"  PR #{pr_number}: {state or 'UNKNOWN'}")

        if state != "MERGED":
            unmerged.append(pr_number)

    return len(unmerged) == 0, unmerged


def write_github_output(
    output_path: str,
    dependency_prs: list[str],
    all_merged: bool,
    unmerged_prs: list[str],
) -> None:
    """Write results to the GITHUB_OUTPUT file."""
    with open(output_path, "a", encoding="utf-8") as f:
        f.write(f"dependency_prs={','.join(dependency_prs)}\n")
        f.write(f"dependency_count={len(dependency_prs)}\n")
        f.write(f"all_dependencies_merged={'true' if all_merged else 'false'}\n")
        f.write(f"unmerged_prs={','.join(unmerged_prs)}\n")


def main() -> int:
    event_name = os.environ.get("EVENT_NAME", "")
    pr_number = os.environ.get("PR_NUMBER", "")
    output_path = os.environ.get("GITHUB_OUTPUT", os.devnull)

    if event_name not in ("pull_request", "pull_request_target", "merge_group"):
        print("Not a pull request event, skipping dependency merge check.")
        write_github_output(output_path, [], True, [])
        return 0

    if not pr_number:
        print("Could not determine PR number.")
        write_github_output(output_path, [], True, [])
        return 0

    print(f"Checking PR #{pr_number} for dependencies...")

    try:
        pr_body = get_pr_body(pr_number)
    except RuntimeError as exc:
        print(f"::error::{exc}")
        write_github_output(output_path, [], True, [])
        return 1

    if not pr_body:
        print("PR description is empty, no dependencies to check.")
        write_github_output(output_path, [], True, [])
        return 0

    dependency_prs = parse_depends_on(pr_body)

    if not dependency_prs:
        print("No 'Depends-On:' keyword found in PR description.")
        write_github_output(output_path, [], True, [])
        return 0

    print(
        f"Found {len(dependency_prs)} dependency PR(s): "
        f"{','.join(dependency_prs)}"
    )
    print("Checking merge status...")

    all_merged, unmerged_prs = check_dependencies(dependency_prs)
    write_github_output(output_path, dependency_prs, all_merged, unmerged_prs)

    if all_merged:
        print(f"✅ All {len(dependency_prs)} dependency PR(s) are merged.")
        return 0

    print(
        f"❌ {len(unmerged_prs)} dependency PR(s) not yet merged: "
        f"{', '.join(f'#{pr}' for pr in unmerged_prs)}"
    )
    print("This PR cannot be merged until all dependency PRs are merged.")
    return 1


if __name__ == "__main__":
    sys.exit(main())

