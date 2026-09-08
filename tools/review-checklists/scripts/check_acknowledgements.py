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

"""Verify that all relevant checklists have been acknowledged by every approving
reviewer, and set the commit status accordingly.

An acknowledgement is a reply in the threaded conversation of a checklist
review comment (finding) that contains the ``OK`` keyword.  This script:

1. Ensures the merge-queue notice (comment + PR description) is present if
   the PR is currently enqueued in the merge queue.
2. Enumerates relevant checklists for the PR.
3. For each checklist, finds the bot-posted review comment and its OK replies.
4. Builds a mapping: checklist-id → set of reviewers who said OK.
5. Compares against the set of approving reviewers.
6. Sets commit status to *success* only when the PR is fully approved per
   branch protection (GitHub's ``reviewDecision``) **and** every approving
   reviewer has acknowledged every relevant checklist. Otherwise sets
   *pending* or *failure*.

This script is invoked on every checklist-relevant event (it is always the
last step run), so it doubles as the single, fully stateless source of
truth for acknowledgement status: it always re-scans the current live
comment state rather than relying on which specific event triggered it.

For ``merge_group`` events it does not merely assume the evidence is still
valid because a required status check passed at some earlier point: it
resolves the underlying pull request and re-runs the same acknowledgement
validation against its current live state, setting the commit status on
the merge-queue's merge-commit SHA. This closes the window between when
the PR's own check last went green and when it actually entered the queue
(and rejects entries where that PR can no longer be resolved at all).
"""

from __future__ import annotations

import argparse
import json
import os
import sys
from typing import Any

from helpers import (
    build_evidence_block,
    collect_acknowledgement_details,
    find_existing_checklist_comments,
    find_ok_replies_for_checklists,
    get_approving_reviewers,
    get_changed_files,
    get_github_client,
    get_repo_and_pr,
    get_review_decision,
    load_checklists,
    match_checklists,
    refresh_merge_queue_notice,
    set_commit_status,
    update_pr_description_with_evidence,
)


def _collect_ok_acknowledgements(
    pr: Any, existing_comments: dict[str, Any], posted_relevant_ids: list[str]
) -> dict[str, set[str]]:
    """Return a mapping of checklist_id → set of usernames who acknowledged.

    A reply counts as an OK for a checklist if:
      - Its ``in_reply_to_id`` matches the checklist finding comment id, AND
      - Its body (stripped, case-insensitive) equals the ``OK`` keyword.

    The conversation thread itself associates the reply with the checklist.
    """
    replies = find_ok_replies_for_checklists(pr, existing_comments, posted_relevant_ids)
    return {checklist_id: {comment.user.login for comment in comments} for checklist_id, comments in replies.items()}


def _acknowledgement_status(
    approvers: list[str],
    posted_relevant_ids: list[str],
    acks: dict[str, set[str]],
    review_decision: str | None,
) -> tuple[str, str]:
    """Compute the (state, description) commit-status pair from ack data.

    Pure decision logic with no side effects, so it can be reused both for
    the live PR flow (which also refreshes evidence/comments) and for
    merge_group evidence validation (which must not write anything).

    ``review_decision`` must be GitHub's own ``"APPROVED"`` verdict (the PR
    currently satisfies its required-review branch-protection rules) before
    this can go green. Without this gate, status could turn "success" from
    only the *current* approver set (e.g. 1 of 2 required approvals), and a
    still-outstanding required reviewer's later approval would instantly
    satisfy GitHub's native review-count check against that stale, already
    green status — before this (slower, two-stage) workflow re-runs to
    verify the new reviewer also acknowledged every checklist. Requiring
    "APPROVED" first means the status can only go green once no further
    required review can arrive to race against it.
    """
    if review_decision != "APPROVED":
        return "pending", "Awaiting full branch-protection approval"

    if not approvers:
        return "pending", "Awaiting at least one approving review"

    missing: dict[str, list[str]] = {}
    for checklist_id in posted_relevant_ids:
        not_acked = [username for username in approvers if username not in acks[checklist_id]]
        if not_acked:
            missing[checklist_id] = not_acked

    if missing:
        summary_parts = [f"{checklist_id}: awaiting {', '.join(users)}" for checklist_id, users in missing.items()]
        return "pending", "; ".join(summary_parts)

    return "success", "All checklists acknowledged by all approving reviewers"


def _validate_checklist_evidence(pr: Any, checklists: list[dict]) -> tuple[str, str]:
    """Re-derive and validate checklist acknowledgement evidence for a PR.

    Reads the PR's current live state (changed files, checklist findings,
    threaded OK replies, approving reviews) and returns the same
    (state, description) pair that would be used to set the
    "review-checklists" commit status, without any side effects. Used to
    validate the evidence at merge_group time instead of assuming it is
    still valid because a required status check passed at some earlier
    point.
    """
    changed_files = get_changed_files(pr)
    relevant_checklists = match_checklists(checklists, changed_files)

    if not relevant_checklists:
        return "success", "No checklists applicable"

    existing = find_existing_checklist_comments(pr)
    posted_relevant_ids = [checklist["id"] for checklist in relevant_checklists if checklist["id"] in existing]

    if len(posted_relevant_ids) < len(relevant_checklists):
        # At least one relevant checklist has no posted comment yet — do not
        # silently drop it from consideration (that could let the status go
        # to "success" while a checklist was never even posted); stay
        # pending until every relevant checklist has been posted.
        return "pending", "Checklist comments not yet posted"

    acks = _collect_ok_acknowledgements(pr, existing, posted_relevant_ids)
    approvers = get_approving_reviewers(pr)
    review_decision = get_review_decision(pr)
    return _acknowledgement_status(approvers, posted_relevant_ids, acks, review_decision)


def main() -> None:
    parser = argparse.ArgumentParser(description="Verify review-checklist acknowledgements on a PR.")
    parser.add_argument(
        "--config-path",
        default=".github/review_checklists.yml",
        help="Path to checklist configuration file (default: .github/review_checklists.yml)",
    )
    args = parser.parse_args()

    gh = get_github_client()
    event_name = os.environ.get("CHECKLISTS_EVENT_NAME", "")
    if event_name == "merge_group":
        head_sha = os.environ.get("HEAD_SHA")
        if not head_sha:
            print("HEAD_SHA is required for merge_group events.")
            sys.exit(1)
        repo_name = os.environ["GITHUB_REPOSITORY"]
        repo = gh.get_repo(repo_name)

        pr_number_raw = os.environ.get("PR_NUMBER", "")
        try:
            pr_number = int(pr_number_raw)
        except ValueError:
            pr_number = 0

        if not pr_number:
            description = "Could not resolve pull request to validate checklist evidence"
            print(description)
            set_commit_status(repo, head_sha, "failure", description)
            sys.exit(1)

        pr = repo.get_pull(pr_number)
        checklists = load_checklists(args.config_path)
        state, description = _validate_checklist_evidence(pr, checklists)
        set_commit_status(repo, head_sha, state, f"Merge queue: {description}")
        if state != "success":
            print(f"Merge-queue checklist evidence validation failed: {description}")
            sys.exit(1)
        print("Merge-queue checklist evidence validated ✅")
        return

    repo, pr = get_repo_and_pr(gh)

    existing = find_existing_checklist_comments(pr)

    # Note this is intentionally separate from the `merge_group` branch
    # above (which already implies the PR is in the queue and instead
    # re-validates evidence and returns/exits). This check instead covers
    # the case where a *different*, non-merge_group event (e.g. a review
    # comment posted on a PR that happens to still be enqueued) fires while
    # the PR is enqueued, so we can (re-)post the advisory merge-queue
    # notice for it.
    refresh_merge_queue_notice(pr, existing)

    checklists = load_checklists(args.config_path)
    changed_files = get_changed_files(pr)
    relevant_checklists = match_checklists(checklists, changed_files)

    if not relevant_checklists:
        set_commit_status(repo, pr.head.sha, "success", "No checklists applicable")
        return

    posted_relevant_ids = [checklist["id"] for checklist in relevant_checklists if checklist["id"] in existing]

    if len(posted_relevant_ids) < len(relevant_checklists):
        # At least one relevant checklist has no posted comment yet — keep
        # pending rather than silently dropping it from consideration (see
        # _validate_checklist_evidence for the same rationale).
        set_commit_status(
            repo,
            pr.head.sha,
            "pending",
            "Checklist comments not yet posted",
        )
        return

    acks = _collect_ok_acknowledgements(pr, existing, posted_relevant_ids)

    # Refresh evidence block in PR description based on current acknowledgements.
    ack_details = collect_acknowledgement_details(pr, existing, posted_relevant_ids)
    evidence_block = build_evidence_block(relevant_checklists, ack_details)
    update_pr_description_with_evidence(pr, evidence_block)

    approvers = get_approving_reviewers(pr)
    review_decision = get_review_decision(pr)
    state, description = _acknowledgement_status(approvers, posted_relevant_ids, acks, review_decision)
    set_commit_status(repo, pr.head.sha, state, description)
    print(f"Acknowledgement status: {state} — {description}")

    # Write acknowledgement data for downstream use (merge evidence).
    ack_data = {checklist_id: sorted(users) for checklist_id, users in acks.items()}
    runner_temp = os.environ.get("RUNNER_TEMP", "./")
    output_path = os.environ.get("ACK_OUTPUT_PATH", runner_temp + "/checklist_acks.json")
    with open(output_path, "w") as f:
        json.dump(ack_data, f, indent=2)
    print(f"Acknowledgement data written to {output_path}")


if __name__ == "__main__":
    main()
