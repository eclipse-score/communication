# *******************************************************************************
# Copyright (c) 2024 Contributors to the Eclipse Foundation
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

"""Shared helpers for review-checklist scripts."""

from __future__ import annotations

import os
from typing import Any

import pathspec
import yaml
from github import Github
from github.PullRequest import PullRequest

# Marker prefix used to identify bot-managed checklist reviews.
CHECKLIST_MARKER = "<!-- review-checklist:{checklist_id} -->"

# The keyword a reviewer must post to acknowledge a checklist.
OK_KEYWORD = "OK"


def _get_github_token() -> str:
    """Return the GitHub token."""
    return os.environ["GITHUB_TOKEN"]


def get_github_client() -> Github:
    """Return an authenticated PyGithub client."""
    token = _get_github_token()
    return Github(token)


def get_repo_and_pr(gh: Github) -> tuple[Any, PullRequest]:
    """Return the repository and pull-request objects from environment."""
    repo_name = os.environ["GITHUB_REPOSITORY"]
    pr_number = int(os.environ["PR_NUMBER"])
    repo = gh.get_repo(repo_name)
    pr = repo.get_pull(pr_number)
    return repo, pr


def _find_checklists_config(
    config_relpath: str = ".github/review_checklists.yml",
) -> str:
    """Locate checklist config via runfiles or path heuristics.

    Args:
        config_relpath: Relative path to the checklist config file.
                        Defaults to '.github/review_checklists.yml'.

    Returns:
        Absolute path to the config file.

    Raises:
        FileNotFoundError: If the config file cannot be located.
    """
    # 1. Bazel runfiles via the bazel-runfiles library (Rlocation API).
    try:
        from runfiles import Runfiles  # type: ignore[import-untyped]

        runfiles = Runfiles.Create()
        if runfiles:
            candidate = runfiles.Rlocation(f"score_communication/{config_relpath}")
            if candidate and os.path.isfile(candidate):
                return candidate
    except (ImportError, Exception):
        pass

    # 2. Fallback: relative to working directory (works outside Bazel).
    if os.path.isfile(config_relpath):
        return config_relpath

    raise FileNotFoundError(f"Cannot locate {config_relpath}")


def load_checklists(
    config_relpath: str = ".github/review_checklists.yml",
) -> list[dict]:
    """Load checklist definitions from the YAML configuration file.

    Args:
        config_relpath: Relative path to the checklist config file.
                        Defaults to '.github/review_checklists.yml'.

    Returns:
        List of checklist definitions.
    """
    config_path = _find_checklists_config(config_relpath)
    with open(config_path, "r") as f:
        data = yaml.safe_load(f)
    return data["checklists"]


def get_changed_files(pr: PullRequest) -> list[str]:
    """Return the list of files changed in the pull request."""
    return [f.filename for f in pr.get_files()]


def _file_matches_patterns(filepath: str, patterns: list[str]) -> bool:
    """Return True if filepath matches any of the given glob patterns.

    Patterns use ``.gitignore``-style ("gitwildmatch") syntax via the
    ``pathspec`` library, not plain ``fnmatch``:
      - A pattern without a leading ``/`` matches at any depth
        (``"*.md"`` matches both ``NOTE.md`` and ``docs/NOTE.md``).
      - A pattern with a leading ``/`` is anchored to the repo root
        (``"/*.md"`` matches only root-level ``.md`` files).
      - ``**`` explicitly matches zero or more path segments
        (``"docs/**"`` matches everything under ``docs/``;
        ``"**/BUILD"`` matches ``BUILD`` at any depth, including the root).
    """
    if not patterns:
        return False
    spec = pathspec.PathSpec.from_lines("gitwildmatch", patterns)
    return spec.match_file(filepath)


def match_checklists(checklists: list[dict], changed_files: list[str]) -> list[dict]:
    """Return checklists whose path patterns match at least one changed file.

    Each checklist may have:
      ``include``: list of glob patterns; a file must match at least one.
      ``exclude``: (optional) list of glob patterns; matching files are removed.

    Each returned checklist dict is augmented with a ``matched_files`` key
    containing the list of changed files that triggered the match.
    """
    relevant_checklists = []
    for checklist in checklists:
        include_patterns: list[str] = checklist.get("include", [])
        exclude_patterns: list[str] = checklist.get("exclude", [])

        matched = set()
        for filepath in changed_files:
            if _file_matches_patterns(filepath, include_patterns):
                if not _file_matches_patterns(filepath, exclude_patterns):
                    matched.add(filepath)

        if matched:
            checklist_copy = dict(checklist)
            checklist_copy["matched_files"] = sorted(matched)
            relevant_checklists.append(checklist_copy)
    return relevant_checklists


def make_checklist_comment_body(checklist: dict) -> str:
    """Build the Markdown body for a checklist PR review comment (finding)."""
    marker = CHECKLIST_MARKER.format(checklist_id=checklist["id"])
    include_patterns = checklist.get("include", [])
    exclude_patterns = checklist.get("exclude", [])
    exclude_line = f"**Excluding files matching:** `{'`, `'.join(exclude_patterns)}`\n\n" if exclude_patterns else ""
    body = (
        f"{marker}\n"
        f"## 📋 {checklist['name']}\n\n"
        f"**Checklist ID:** `{checklist['id']}`\n\n"
        f"**Applicable to files matching:** `{'`, `'.join(include_patterns)}`\n\n"
        f"{exclude_line}"
        f"### Checklist\n\n"
        f"{checklist['checklist'].strip()}\n\n"
        f"---\n"
        f"**To acknowledge this checklist, reply to this conversation "
        f"with exactly `{OK_KEYWORD}`.** Each approving reviewer must "
        f"acknowledge every applicable checklist before the PR can be merged.\n"
    )
    return body


def find_existing_checklist_comments(pr: PullRequest) -> dict[str, Any]:
    """Find existing bot-managed checklist review comments (findings) on the PR.

    Returns a dict mapping checklist-id → PullRequestComment object.

    Checklist findings are identified by the ``CHECKLIST_MARKER`` HTML comment
    in their body.  We search PR review comments (``get_review_comments()``)
    because checklists are posted as file-level review comments that support
    threaded conversations where reviewers can reply with OK.
    """
    result = {}
    for comment in pr.get_review_comments():
        body = comment.body or ""
        prefix = "<!-- review-checklist:"
        if prefix in body:
            start = body.index(prefix) + len(prefix)
            end = body.find(" -->", start)
            if end == -1:
                # Marker was opened but never closed (e.g. comment edited/
                # truncated by a user) — skip rather than crash on the
                # malformed comment.
                continue
            checklist_id = body[start:end]
            # Only keep top-level checklist comments (not replies).
            if not getattr(comment, "in_reply_to_id", None):
                result[checklist_id] = comment
    return result


def find_ok_replies_for_checklists(
    pr: PullRequest,
    existing_comments: dict[str, Any],
    checklist_ids: list[str],
) -> dict[str, list[Any]]:
    """Return a mapping of checklist_id -> its OK-reply comment objects.

    Single-pass helper over ``pr.get_review_comments()`` that consolidates
    what used to be three separate re-implementations of the same OK-reply
    lookup (``collect_acknowledgement_details`` here,
    ``check_acknowledgements._collect_ok_acknowledgements``, and
    ``dismiss_and_invalidate._find_ok_comments_for_checklist``): a reply
    counts as an OK for a checklist if its ``in_reply_to_id`` matches that
    checklist's finding comment id and its body (stripped,
    case-insensitive) equals the OK keyword. Callers project this
    "superset" of full comment objects down to whatever narrower shape
    they actually need (usernames, (reviewer, timestamp) pairs, raw
    comment objects to delete, ...).
    """
    replies: dict[str, list[Any]] = {checklist_id: [] for checklist_id in checklist_ids}
    comment_id_to_checklist_id: dict[int, str] = {
        comment.id: checklist_id for checklist_id, comment in existing_comments.items() if checklist_id in checklist_ids
    }

    for comment in pr.get_review_comments():
        reply_to = getattr(comment, "in_reply_to_id", None)
        if not isinstance(reply_to, int) or reply_to not in comment_id_to_checklist_id:
            continue
        if (comment.body or "").strip().upper() != OK_KEYWORD:
            continue
        replies[comment_id_to_checklist_id[reply_to]].append(comment)

    return replies


def collect_acknowledgement_details(
    pr: PullRequest, existing_comments: dict[str, Any], checklist_ids: list[str]
) -> dict[str, list[dict[str, str]]]:
    """Return acknowledgement details for relevant checklist review threads."""
    replies = find_ok_replies_for_checklists(pr, existing_comments, checklist_ids)
    return {
        checklist_id: [
            {
                "reviewer": comment.user.login,
                "acknowledged_at": comment.created_at.isoformat(),
            }
            for comment in comments
        ]
        for checklist_id, comments in replies.items()
    }


def get_approving_reviewers(pr: PullRequest) -> list[str]:
    """Return a list of usernames who have an active APPROVED review."""
    approvers = set()
    for review in pr.get_reviews():
        if review.state == "APPROVED":
            approvers.add(review.user.login)
        elif review.state in ("CHANGES_REQUESTED", "DISMISSED"):
            # get_reviews() returns every review a user has ever submitted on
            # this PR, in submission order — not just their latest state. A
            # user can approve and later request changes (or have an
            # approval dismissed) in a subsequent review, so this discard is
            # what removes a since-superseded approval from an earlier
            # iteration of this loop; it is not a no-op.
            approvers.discard(review.user.login)
    return sorted(approvers)


# GitHub's commit-status API silently truncates/rejects descriptions longer
# than this; keep our own text within the limit explicitly.
COMMIT_STATUS_DESCRIPTION_MAX_LENGTH = 140


def set_commit_status(
    repo: Any,
    sha: str,
    state: str,
    description: str,
    context: str = "review-checklists",
) -> None:
    """Set a commit status on the given SHA."""
    desc = description[:COMMIT_STATUS_DESCRIPTION_MAX_LENGTH]
    print(f"Setting commit status: context='{context}', state='{state}', sha='{sha}', description='{desc}'")
    repo.get_commit(sha).create_status(
        state=state,
        description=desc,
        context=context,
    )
    print("Commit status set successfully.")


# Evidence block markers for PR description
EVIDENCE_BLOCK_START = "<!-- review-checklist-evidence:start -->"
EVIDENCE_BLOCK_END = "<!-- review-checklist-evidence:end -->"

# Standalone merge-queue notice block in PR description.
MERGE_QUEUE_NOTICE_START = "<!-- review-checklist-merge-queue-notice:start -->"
MERGE_QUEUE_NOTICE_END = "<!-- review-checklist-merge-queue-notice:end -->"

# Marker for a bot-managed PR comment carrying the same notice.
MERGE_QUEUE_COMMENT_MARKER = "<!-- review-checklist-merge-queue-comment -->"

MERGE_QUEUE_NOTICE = [
    MERGE_QUEUE_NOTICE_START,
    "## Review Checklist Evidence Notice - Merge Queue",
    "",
    "This pull request was modified after the review checklist evidence was recorded.",
    "The review checklist evidence visible here does no longer reflect the evidence that will be recorded at merge.",
    "Please rely on the evidence in the git history once the pull request was merged.",
    "",
    "The git history shows the evidence state at the time of merge queue entry.",
    "A pull request may only enter the merge queue when all necessary review checklist acknowledgements are in place.",
    "Changes made after this pull request enters the merge queue may update the evidence here,",
    "but they do not affect the evidence recorded in the git history.",
    MERGE_QUEUE_NOTICE_END,
]


def extract_evidence_block(description: str) -> str | None:
    """Extract the evidence block from PR description, or None if not present."""
    if EVIDENCE_BLOCK_START not in description:
        return None
    try:
        start = description.index(EVIDENCE_BLOCK_START)
        end = description.index(EVIDENCE_BLOCK_END)
        return description[start : end + len(EVIDENCE_BLOCK_END)]
    except ValueError:
        return None


def remove_evidence_block(description: str) -> str:
    """Remove the evidence block from PR description."""
    if EVIDENCE_BLOCK_START not in description:
        return description
    try:
        start = description.index(EVIDENCE_BLOCK_START)
        end = description.index(EVIDENCE_BLOCK_END) + len(EVIDENCE_BLOCK_END)
        # Remove the evidence block and any trailing whitespace
        result = description[:start] + description[end:]
        return result.rstrip() + "\n"
    except ValueError:
        return description


def build_evidence_block(
    relevant_checklists: list[dict],
    ack_details: dict[str, list[dict[str, str]]],
) -> str:
    """Build the evidence block for the PR description."""
    from datetime import datetime, timezone

    lines = [
        EVIDENCE_BLOCK_START,
        "<details>",
        "<summary>Checklist Report (do not modify)</summary>",
        "",
        "## Review Checklist Evidence",
        "",
        f"**Last updated:** {datetime.now(timezone.utc).isoformat()}",
        "",
    ]

    for checklist in relevant_checklists:
        checklist_id = checklist["id"]
        lines.append(f"### {checklist['name']} (`{checklist_id}`)")
        lines.append("")

        acks = ack_details.get(checklist_id, [])
        if acks:
            lines.append("**Acknowledged by:**")
            for ack in acks:
                lines.append(f"- {ack['reviewer']} at {ack['acknowledged_at']}")
        else:
            lines.append("**Acknowledged by:** No acknowledgements yet")
        lines.append("")

    lines += ["</details>", EVIDENCE_BLOCK_END]
    return "\n".join(lines)


def update_pr_description_with_evidence(
    pr: Any,
    evidence_block: str,
) -> None:
    """Update PR description to include/replace evidence block."""
    current_description = pr.body or ""

    # Remove existing evidence block
    new_description = remove_evidence_block(current_description)

    # Append new evidence block
    new_description = new_description + "\n" + evidence_block

    # Only update if description changed
    if new_description.strip() != current_description.strip():
        pr.edit(body=new_description)
        print("Updated PR description with evidence block")
    else:
        print("PR description evidence is already up to date")


def is_pr_in_merge_queue(pr: Any) -> bool:
    """Return whether the PR is currently in GitHub merge queue via GraphQL.

    This is intentionally fail-closed (returns False) on any lookup error
    rather than raising: its only callers use the result to decide whether
    to post an advisory merge-queue notice on the PR, and a false negative
    here at worst delays that notice until the next checklist-relevant
    event — it never affects merge gating. The actual merge-queue gating
    decision is made independently in check_acknowledgements.py's
    ``merge_group`` handling, which resolves and validates the underlying
    PR itself and fails the commit status (does not default to success)
    if that resolution or validation fails.
    """
    repo_name = getattr(getattr(getattr(pr, "base", None), "repo", None), "full_name", "")
    if not repo_name or "/" not in repo_name:
        repo_name = os.environ.get("GITHUB_REPOSITORY", "")
    if "/" not in repo_name:
        print("Could not determine repository for merge-queue lookup")
        return False

    number = getattr(pr, "number", None)
    if not number:
        try:
            number = int(os.environ.get("PR_NUMBER", "0"))
        except ValueError:
            number = 0
    if not number:
        print("Could not determine PR number for merge-queue lookup")
        return False

    owner, name = repo_name.split("/", 1)
    query = """
      query($owner: String!, $name: String!, $number: Int!) {
        repository(owner: $owner, name: $name) {
          pullRequest(number: $number) {
            isInMergeQueue
          }
        }
      }
    """

    try:
        result = _run_graphql_query(
            query,
            {"owner": owner, "name": name, "number": int(number)},
        )
    except Exception as exc:
        print(f"GraphQL merge-queue lookup failed: {exc}")
        return False

    is_in_queue = result.get("data", {}).get("repository", {}).get("pullRequest", {}).get("isInMergeQueue")
    if isinstance(is_in_queue, bool):
        return is_in_queue

    print("GraphQL merge-queue lookup returned no boolean state")
    return False


def _build_merge_queue_notice_block() -> str:
    """Return the standalone merge-queue notice for PR description."""
    return "\n".join(MERGE_QUEUE_NOTICE)


def _remove_merge_queue_notice_block(description: str) -> str:
    """Remove the standalone merge-queue notice block from PR description."""
    if MERGE_QUEUE_NOTICE_START not in description:
        return description
    try:
        start = description.index(MERGE_QUEUE_NOTICE_START)
        end = description.index(MERGE_QUEUE_NOTICE_END) + len(MERGE_QUEUE_NOTICE_END)
        result = description[:start] + description[end:]
        return result.rstrip() + "\n"
    except ValueError:
        return description


def ensure_merge_queue_notice_description(pr: Any) -> None:
    """Ensure a standalone merge-queue notice exists in the PR description."""
    current_description = pr.body or ""
    notice_block = _build_merge_queue_notice_block()
    description_without_notice = _remove_merge_queue_notice_block(current_description)
    base = description_without_notice.rstrip()
    if base:
        new_description = base + "\n\n" + notice_block
    else:
        new_description = notice_block

    if new_description.strip() != current_description.strip():
        pr.edit(body=new_description)
        print("Updated PR description with merge-queue evidence notice")


def ensure_merge_queue_notice_comment(pr: Any) -> None:
    """Ensure the PR has a single bot-managed merge-queue evidence notice."""
    body = "\n".join([MERGE_QUEUE_COMMENT_MARKER] + MERGE_QUEUE_NOTICE)

    existing_comment = None
    for comment in pr.get_issue_comments():
        comment_body = comment.body or ""
        if MERGE_QUEUE_COMMENT_MARKER in comment_body:
            existing_comment = comment
            break

    if existing_comment is None:
        pr.create_issue_comment(body)
        print("Posted merge-queue evidence notice comment")
        return

    if (existing_comment.body or "").strip() != body.strip():
        existing_comment.edit(body)
        print("Updated merge-queue evidence notice comment")


def _run_graphql_query(query: str, variables: dict[str, Any]) -> dict[str, Any]:
    """Execute a GitHub GraphQL query via gql and return JSON-like data."""
    from gql import Client, gql
    from gql.transport.requests import RequestsHTTPTransport

    token = _get_github_token()
    transport = RequestsHTTPTransport(
        url="https://api.github.com/graphql",
        headers={
            "Authorization": f"Bearer {token}",
            "Accept": "application/vnd.github+json",
        },
        use_json=True,
    )
    client = Client(transport=transport, fetch_schema_from_transport=False)
    data = client.execute(gql(query), variable_values=variables)

    if not isinstance(data, dict):
        raise RuntimeError("Unexpected GraphQL response type")
    return {"data": data}
