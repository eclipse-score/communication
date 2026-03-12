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
rebase_on_prs.py
Rebases the current branch onto dependency PR branches.

Accepts a comma-separated list of PR numbers as a CLI argument.
Requires GH_TOKEN environment variable for GitHub CLI authentication.
Writes rebased=, rebased_count=, skipped_prs=, failed_prs= directly to $GITHUB_OUTPUT.
"""

import os
import subprocess
import sys

# Return codes for rebase_on_pr
REBASE_SUCCESS = 0
REBASE_SKIPPED = 1
REBASE_FAILED = 2


def get_pr_field(pr_number: str, field: str) -> str:
    """Get a single field from a PR using the GitHub CLI."""
    result = subprocess.run(
        ["gh", "pr", "view", pr_number, "--json", field, "--jq", f".{field}"],
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        return ""
    return result.stdout.strip()


def rebase_on_pr(pr_number: str) -> int:
    """Perform rebase onto a single dependency PR.

    Returns:
        REBASE_SUCCESS (0) on successful rebase
        REBASE_SKIPPED (1) when the PR is merged or closed
        REBASE_FAILED  (2) on any error
    """
    print()
    print("==========================================")
    print(f"Processing dependency PR #{pr_number}...")
    print("==========================================")

    dependency_branch = get_pr_field(pr_number, "headRefName")
    if not dependency_branch:
        print(
            f"::error::Could not fetch branch name for PR #{pr_number}. "
            "Make sure the PR exists and is open."
        )
        return REBASE_FAILED

    print(f"Dependency PR branch: {dependency_branch}")

    dependency_state = get_pr_field(pr_number, "state")

    if dependency_state == "MERGED":
        print(f"Dependency PR #{pr_number} has been merged. Skipping.")
        return REBASE_SKIPPED

    if dependency_state == "CLOSED":
        print(
            f"::warning::Dependency PR #{pr_number} is closed but not merged. "
            "Consider removing the Depends-On reference."
        )
        return REBASE_SKIPPED

    # Use refs/pull/<pr>/head so fork PRs are supported: GitHub exposes the
    # head commit of every PR at this ref on the base repository, regardless
    # of whether the head lives on a fork or the same repo.
    pr_ref = f"refs/pull/{pr_number}/head"
    local_ref = f"refs/remotes/origin/pr/{pr_number}"

    print(f"Fetching PR #{pr_number} via {pr_ref} (fork-compatible)...")
    fetch_result = subprocess.run(["git", "fetch", "origin", f"{pr_ref}:{local_ref}"])
    if fetch_result.returncode != 0:
        print(f"::error::Failed to fetch PR #{pr_number}.")
        return REBASE_FAILED

    print(f"Rebasing current branch onto {local_ref} (branch: {dependency_branch})...")
    rebase_result = subprocess.run(["git", "rebase", local_ref])
    if rebase_result.returncode == 0:
        print(f"Rebase onto PR #{pr_number} successful!")
        return REBASE_SUCCESS

    print(f"::error::Rebase failed for PR #{pr_number}. There may be conflicts.")
    subprocess.run(["git", "rebase", "--abort"])
    return REBASE_FAILED


def write_github_output(
    output_path: str,
    rebased: bool,
    rebased_count: int,
    skipped_prs: list[str],
    failed_prs: list[str],
) -> None:
    """Write rebase results to the GITHUB_OUTPUT file."""
    with open(output_path, "a", encoding="utf-8") as f:
        f.write(f"rebased={'true' if rebased else 'false'}\n")
        f.write(f"rebased_count={rebased_count}\n")
        f.write(f"skipped_prs={','.join(skipped_prs)}\n")
        f.write(f"failed_prs={','.join(failed_prs)}\n")


def main(dependency_prs_str: str) -> int:
    output_path = os.environ.get("GITHUB_OUTPUT", os.devnull)

    rebased_count = 0
    skipped_prs: list[str] = []
    failed_prs: list[str] = []

    # Configure git user for the rebase
    subprocess.run(["git", "config", "user.name", "github-actions[bot]"])
    subprocess.run(
        ["git", "config", "user.email", "github-actions[bot]@users.noreply.github.com"]
    )

    pr_numbers = [p.strip() for p in dependency_prs_str.split(",") if p.strip()]

    for pr_number in pr_numbers:
        result = rebase_on_pr(pr_number)
        if result == REBASE_SUCCESS:
            rebased_count += 1
        elif result == REBASE_SKIPPED:
            skipped_prs.append(pr_number)
        elif result == REBASE_FAILED:
            failed_prs.append(pr_number)

    write_github_output(output_path, rebased_count > 0, rebased_count, skipped_prs, failed_prs)

    print()
    print("==========================================")
    print(f"Summary: Rebased onto {rebased_count} PR(s)")
    if skipped_prs:
        print(f"Skipped: {','.join(skipped_prs)}")
    if failed_prs:
        print(f"Failed: {','.join(failed_prs)}")
    print("==========================================")

    return 1 if failed_prs else 0


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: rebase_on_prs.py <comma_separated_pr_numbers>", file=sys.stderr)
        sys.exit(1)
    sys.exit(main(sys.argv[1]))

