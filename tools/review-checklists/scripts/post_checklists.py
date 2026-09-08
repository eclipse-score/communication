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

"""Post or update review-checklist findings on a pull request.

For every relevant checklist (determined by path-matching against changed
files), a file-level PR review comment (finding) is created on the PR,
anchored to the first matched file.  If the finding already exists it is
updated in place so that the conversation thread (and any replies) is
preserved.  File-level review comments are used because they support
threaded conversations where reviewers can reply directly with OK.
"""

from __future__ import annotations

import argparse

from helpers import (
    build_evidence_block,
    collect_acknowledgement_details,
    find_existing_checklist_comments,
    get_changed_files,
    get_github_client,
    get_repo_and_pr,
    load_checklists,
    make_checklist_comment_body,
    match_checklists,
    set_commit_status,
    update_pr_description_with_evidence,
)


def main() -> None:
    parser = argparse.ArgumentParser(description="Post or update review-checklist findings on a PR.")
    parser.add_argument(
        "--config-path",
        default=".github/review_checklists.yml",
        help="Path to checklist configuration file (default: .github/review_checklists.yml)",
    )
    args = parser.parse_args()

    gh = get_github_client()
    repo, pr = get_repo_and_pr(gh)

    checklists = load_checklists(args.config_path)
    changed_files = get_changed_files(pr)
    relevant_checklists = match_checklists(checklists, changed_files)

    if not relevant_checklists:
        print("No checklists are relevant for this PR.")
        set_commit_status(
            repo,
            pr.head.sha,
            "success",
            "No checklists applicable",
        )
        return

    existing = find_existing_checklist_comments(pr)

    for checklist in relevant_checklists:
        body = make_checklist_comment_body(checklist)
        if checklist["id"] in existing:
            comment = existing[checklist["id"]]
            # Only update if the body actually changed (avoids notification spam).
            if (comment.body or "").strip() != body.strip():
                comment.edit(body=body)
                print(f"Updated checklist finding for '{checklist['id']}'")
            else:
                print(f"Checklist finding for '{checklist['id']}' is already up to date")
        else:
            # Post a file-level review comment (subject_type="file") anchored
            # to the first matched file. Unlike diff-position-anchored
            # comments, file-level comments don't require the file to appear
            # as a text diff hunk, so this also works for binary files and
            # files GitHub doesn't render a diff for. It still creates a
            # PullRequestComment that supports threaded replies where
            # reviewers can acknowledge with OK.
            anchor_file = checklist["matched_files"][0]
            pr.create_review_comment(
                body=body,
                commit=pr.head.sha,
                path=anchor_file,
                subject_type="file",
            )
            print(f"Created checklist finding for '{checklist['id']}'")

    # Collect current acknowledgements and update evidence in PR description
    posted_relevant_ids = [checklist["id"] for checklist in relevant_checklists if checklist["id"] in existing]
    if posted_relevant_ids:
        ack_details = collect_acknowledgement_details(pr, existing, posted_relevant_ids)
        evidence_block = build_evidence_block(relevant_checklists, ack_details)
        update_pr_description_with_evidence(pr, evidence_block)

    # Set a pending check — actual pass/fail is determined by check_acknowledgements.
    set_commit_status(
        repo,
        pr.head.sha,
        "pending",
        f"{len(relevant_checklists)} checklist(s) require reviewer acknowledgement",
    )

    print(f"Posted/updated {len(relevant_checklists)} checklist finding(s).")


if __name__ == "__main__":
    main()
