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
from datetime import datetime, timezone
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


def get_review_decision(pr: Any) -> str | None:
    """Return the PR's branch-protection review decision via GraphQL.

    ``reviewDecision`` is GitHub's own computed verdict on whether the PR
    currently satisfies its required-review branch-protection rules (required
    approving review count, CODEOWNERS, etc.) — one of ``"APPROVED"``,
    ``"REVIEW_REQUIRED"``, or ``"CHANGES_REQUESTED"``. It is not exposed via
    the REST API, hence the GraphQL round-trip.

    Used to gate the "review-checklists" commit status: it must not go
    green from a partial/current approver set alone (see
    ``_acknowledgement_status``) if the PR is not yet fully approved per
    branch protection — otherwise a still-outstanding required reviewer
    could approve later, instantly satisfying GitHub's native review-count
    check against our *already-green*, stale status, before our (slower,
    two-stage) workflow has re-validated that this new reviewer has also
    acknowledged every checklist.

    Returns ``None`` on any lookup failure so callers can fail closed
    (treat as not yet fully approved) rather than silently proceeding.
    """
    owner_and_name = _resolve_repo_owner_and_name(pr)
    if owner_and_name is None:
        print("Could not determine repository for review-decision lookup")
        return None
    owner, name = owner_and_name

    number = _resolve_pr_number(pr)
    if number is None:
        print("Could not determine PR number for review-decision lookup")
        return None

    query = """
      query($owner: String!, $name: String!, $number: Int!) {
        repository(owner: $owner, name: $name) {
          pullRequest(number: $number) {
            reviewDecision
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
        print(f"GraphQL review-decision lookup failed: {exc}")
        return None

    return result.get("data", {}).get("repository", {}).get("pullRequest", {}).get("reviewDecision")


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

_MERGE_QUEUE_NOTICE_HEADER = [
    "## Review Checklist Evidence Notice - Merge Queue",
    "",
    "This pull request is (or recently was) in the merge queue.",
    "A pull request may only enter the merge queue when all necessary review checklist acknowledgements are in place.",
    "",
]

_MERGE_QUEUE_NOTICE_BODY_BY_STATUS = {
    # A human-authored change to checklist evidence (the description,
    # acknowledgement-thread replies, approving reviews, or commits) was
    # detected after this PR entered the merge queue, per GitHub's own
    # event/edit history — not merely inferred from the PR currently being
    # in the queue.
    "modified": [
        "Checklist evidence (the description, an acknowledgement reply, an approving review, or a commit)",
        "changed after this pull request entered the merge queue.",
        "The evidence visible here may no longer reflect what was true at merge-queue entry.",
    ],
    # Could not resolve the merge-queue entry timestamp, so no verdict is
    # made either way — fail-open on the claim itself (never assert
    # "modified" without evidence for it).
    "unknown": [
        "Whether checklist evidence changed since this pull request entered the merge queue could not be determined.",
    ],
}

_MERGE_QUEUE_NOTICE_FOOTER = [
    "",
    "Once this pull request is merged, prefer the evidence recorded in git history over this description",
    "where the two differ — the description keeps updating live for as long as this PR exists, while the",
    "merge commit (if your merge-queue configuration embeds the PR title and description into it) is a",
    "fixed, tamper-evident record of what was true at that point.",
]


def _build_merge_queue_notice_lines(status: str) -> list[str]:
    """Build the merge-queue notice body lines for the given evidence status.

    ``status`` is one of ``"modified"`` or ``"unknown"`` (see
    ``checklist_evidence_modified_since`` / ``get_merge_queue_state``) and
    selects which factual claim the notice makes about whether checklist
    evidence changed since merge-queue entry. Callers never invoke this for
    an ``"unmodified"`` verdict — nothing noteworthy to report means no
    notice is posted or refreshed at all (see ``resolve_merge_queue_evidence_status``
    callers in check_acknowledgements.py/post_checklists.py).
    """
    body = _MERGE_QUEUE_NOTICE_BODY_BY_STATUS.get(status, _MERGE_QUEUE_NOTICE_BODY_BY_STATUS["unknown"])
    return (
        [MERGE_QUEUE_NOTICE_START]
        + _MERGE_QUEUE_NOTICE_HEADER
        + body
        + _MERGE_QUEUE_NOTICE_FOOTER
        + [MERGE_QUEUE_NOTICE_END]
    )


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


def _resolve_repo_owner_and_name(pr: Any) -> tuple[str, str] | None:
    """Return (owner, name) for the PR's repository, or None if unresolvable.

    Prefers the PR object's own base-repo reference; falls back to the
    ``GITHUB_REPOSITORY`` environment variable (as set by Actions) when the
    PR object doesn't carry it (e.g. a mock in tests).
    """
    repo_name = getattr(getattr(getattr(pr, "base", None), "repo", None), "full_name", "")
    if not repo_name or "/" not in repo_name:
        repo_name = os.environ.get("GITHUB_REPOSITORY", "")
    if "/" not in repo_name:
        return None
    owner, name = repo_name.split("/", 1)
    return owner, name


def _resolve_pr_number(pr: Any) -> int | None:
    """Return the PR number, falling back to the ``PR_NUMBER`` env var."""
    number = getattr(pr, "number", None)
    if not number:
        try:
            number = int(os.environ.get("PR_NUMBER", "0"))
        except ValueError:
            number = 0
    return number or None


def get_merge_queue_state(pr: Any) -> tuple[bool, datetime | None]:
    """Return (is_currently_in_queue, latest_enqueued_at) for the PR, via one GraphQL call.

    Reads the ``AddedToMergeQueueEvent``/``RemovedFromMergeQueueEvent``
    timeline: whichever of the two event types is chronologically *last*
    determines current membership (an ``Added`` event with no later
    ``Removed`` event means the PR is still enqueued), while the
    ``createdAt`` of the latest ``AddedToMergeQueueEvent`` gives the
    baseline timestamp comparisons should use (a PR can be dequeued and
    re-enqueued more than once, e.g. after being kicked out for a new push).

    These are real, GitHub-recorded event timestamps — not a live poll of a
    boolean flag — so comparisons against them are not subject to the race
    that made polling ``isInMergeQueue`` alone unreliable for deciding
    whether content changed *after* enqueue (see
    ``checklist_evidence_modified_since``). This single query replaces what
    used to be two separate lookups (a live ``isInMergeQueue`` poll plus a
    separate ``timelineItems`` query for the enqueue timestamp).

    This is intentionally fail-closed (returns ``(False, None)``) on any
    lookup error rather than raising: its only callers use the result to
    decide whether to post an advisory merge-queue notice on the PR, and a
    false negative here at worst delays that notice until the next
    checklist-relevant event — it never affects merge gating. The actual
    merge-queue gating decision is made independently in
    check_acknowledgements.py's ``merge_group`` handling, which resolves and
    validates the underlying PR itself and fails the commit status (does not
    default to success) if that resolution or validation fails.
    """
    owner_and_name = _resolve_repo_owner_and_name(pr)
    if owner_and_name is None:
        print("Could not determine repository for merge-queue timeline lookup")
        return False, None
    owner, name = owner_and_name

    number = _resolve_pr_number(pr)
    if number is None:
        print("Could not determine PR number for merge-queue timeline lookup")
        return False, None

    query = """
      query($owner: String!, $name: String!, $number: Int!) {
        repository(owner: $owner, name: $name) {
          pullRequest(number: $number) {
            timelineItems(
              itemTypes: [ADDED_TO_MERGE_QUEUE_EVENT, REMOVED_FROM_MERGE_QUEUE_EVENT]
              last: 10
            ) {
              nodes {
                __typename
                ... on AddedToMergeQueueEvent {
                  createdAt
                }
                ... on RemovedFromMergeQueueEvent {
                  createdAt
                }
              }
            }
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
        print(f"GraphQL merge-queue timeline lookup failed: {exc}")
        return False, None

    nodes = (
        result.get("data", {}).get("repository", {}).get("pullRequest", {}).get("timelineItems", {}).get("nodes", [])
    )
    events = [
        (node["createdAt"], node.get("__typename"))
        for node in nodes
        if node.get("createdAt") and node.get("__typename")
    ]
    if not events:
        return False, None

    events.sort(key=lambda item: item[0])

    added_timestamps = [created_at for created_at, typename in events if typename == "AddedToMergeQueueEvent"]
    latest_added_at = (
        datetime.strptime(max(added_timestamps), "%Y-%m-%dT%H:%M:%SZ").replace(tzinfo=timezone.utc)
        if added_timestamps
        else None
    )

    is_in_queue = events[-1][1] == "AddedToMergeQueueEvent"
    return is_in_queue, latest_added_at


def _get_pr_body_human_edits_after(pr: Any, since: datetime) -> bool:
    """Return whether a non-bot editor edited the PR description after ``since``.

    Uses GraphQL ``userContentEdits`` — GitHub's own append-only edit-history
    audit trail for the PR description — rather than any state we would have
    to store/protect ourselves. Edits made by this action's own bot account
    (identified by GraphQL ``__typename: Bot``, not by matching a login
    string, so this doesn't need to know the configured bot's name) are
    excluded, since the evidence-block refresh is expected to keep rewriting
    the description on every run and must not itself count as "modified".
    """
    owner_and_name = _resolve_repo_owner_and_name(pr)
    if owner_and_name is None:
        print("Could not determine repository for PR body edit-history lookup")
        return False
    owner, name = owner_and_name

    number = _resolve_pr_number(pr)
    if number is None:
        print("Could not determine PR number for PR body edit-history lookup")
        return False

    query = """
      query($owner: String!, $name: String!, $number: Int!) {
        repository(owner: $owner, name: $name) {
          pullRequest(number: $number) {
            userContentEdits(first: 100) {
              nodes {
                createdAt
                editor {
                  login
                  __typename
                }
              }
            }
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
        print(f"GraphQL PR body edit-history lookup failed: {exc}")
        return False

    edits = (
        result.get("data", {}).get("repository", {}).get("pullRequest", {}).get("userContentEdits", {}).get("nodes", [])
    )
    for edit in edits:
        editor = edit.get("editor") or {}
        if editor.get("__typename") == "Bot":
            continue
        created_at_raw = edit.get("createdAt")
        if not created_at_raw:
            continue
        created_at = datetime.strptime(created_at_raw, "%Y-%m-%dT%H:%M:%SZ").replace(tzinfo=timezone.utc)
        if created_at > since:
            return True
    return False


def checklist_evidence_modified_since(
    pr: Any,
    existing_comments: dict[str, Any],
    enqueued_at: datetime,
) -> bool:
    """Return whether checklist evidence changed after ``enqueued_at``.

    Deliberately scoped to the same primitives ``check_acknowledgements.py``
    already treats as "evidence" — the PR description (which carries the
    evidence block), checklist-thread replies (OK acknowledgements),
    approving reviews, and the commit set — rather than flagging *any*
    human activity on the PR (an unrelated comment or an incidental
    description typo fix elsewhere should not trigger this).

    Every timestamp compared here comes from GitHub's own event/edit
    history (GraphQL ``timelineItems``/``userContentEdits``, REST
    ``updated_at``/``submitted_at``), not from polling a live boolean at an
    arbitrary later time, so this is not subject to the enqueue-ordering
    race that made unconditionally trusting a live ``isInMergeQueue`` poll
    unreliable.
    """
    # 1. PR description (evidence block) edited by a human after enqueue.
    if _get_pr_body_human_edits_after(pr, enqueued_at):
        return True

    # 2. New or edited replies in checklist finding threads (acknowledgements).
    finding_comment_ids = {comment.id for comment in existing_comments.values()}
    for comment in pr.get_review_comments():
        reply_to = getattr(comment, "in_reply_to_id", None)
        if reply_to not in finding_comment_ids:
            continue
        if comment.created_at > enqueued_at:
            return True
        updated_at = getattr(comment, "updated_at", None)
        if updated_at and updated_at > enqueued_at and updated_at != comment.created_at:
            return True

    # 3. Approver set changed (new/changed review submitted after enqueue).
    for review in pr.get_reviews():
        submitted_at = getattr(review, "submitted_at", None)
        if submitted_at and submitted_at > enqueued_at:
            return True

    # 4. New commits pushed after enqueue (defensive: a merge queue normally
    # dequeues a PR on a new push, but this covers that possibility anyway).
    for commit in pr.get_commits():
        commit_date = getattr(getattr(commit.commit, "author", None), "date", None)
        if commit_date and commit_date > enqueued_at:
            return True

    return False


def resolve_merge_queue_evidence_status(
    pr: Any,
    existing_comments: dict[str, Any],
    enqueued_at: datetime | None,
) -> str:
    """Return ``"modified"``, ``"unmodified"``, or ``"unknown"`` for the PR's
    checklist evidence relative to its most recent merge-queue entry.

    Combines ``enqueued_at`` (the real, GitHub-recorded enqueue timestamp,
    from ``get_merge_queue_state``) with ``checklist_evidence_modified_since``
    (which primitives changed after it). Returns ``"unknown"`` — never
    guessing either way — when the enqueue timestamp itself cannot be
    resolved, e.g. because the PR was never observed as enqueued via the
    timeline, or the lookup failed.
    """
    if enqueued_at is None:
        return "unknown"
    return "modified" if checklist_evidence_modified_since(pr, existing_comments, enqueued_at) else "unmodified"


def refresh_merge_queue_notice(pr: Any, existing_comments: dict[str, Any]) -> None:
    """Post/refresh the advisory merge-queue notice (comment + description)
    for the PR, if warranted.

    This is intentionally separate from ``check_acknowledgements.py``'s
    ``merge_group`` handling (which already implies the PR is in the queue
    and instead re-validates evidence and fails/passes the commit status).
    This function instead covers the case of any other event (e.g. a review
    comment posted on a PR that happens to still be enqueued) firing while
    the PR is enqueued, so the advisory notice can be (re-)posted for it.

    No-ops entirely (posts/updates nothing) unless the PR is currently
    enqueued (per ``get_merge_queue_state``) *and* checklist evidence
    changed since enqueue, or we couldn't determine whether it did — an
    "unmodified" verdict doesn't warrant a notice at all, so no comment or
    description update happens for it.

    Shared by ``check_acknowledgements.py`` and ``post_checklists.py``,
    which both need to perform this exact check after handling their
    respective events.
    """
    is_in_queue, enqueued_at = get_merge_queue_state(pr)
    if not is_in_queue:
        return

    status = resolve_merge_queue_evidence_status(pr, existing_comments, enqueued_at)
    if status == "unmodified":
        return

    ensure_merge_queue_notice_comment(pr, status)
    ensure_merge_queue_notice_description(pr, status)


def _build_merge_queue_notice_block(status: str) -> str:
    """Return the standalone merge-queue notice for PR description."""
    return "\n".join(_build_merge_queue_notice_lines(status))


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


def ensure_merge_queue_notice_description(pr: Any, status: str = "unknown") -> None:
    """Ensure a standalone merge-queue notice exists in the PR description.

    ``status`` (one of ``"modified"``, ``"unmodified"``, ``"unknown"``) is
    computed by the caller via ``resolve_merge_queue_evidence_status``
    (which itself relies on ``get_merge_queue_state``/
    ``checklist_evidence_modified_since``) and selects which factual claim
    the notice makes — this function itself performs no detection.
    """
    current_description = pr.body or ""
    notice_block = _build_merge_queue_notice_block(status)
    description_without_notice = _remove_merge_queue_notice_block(current_description)
    base = description_without_notice.rstrip()
    if base:
        new_description = base + "\n\n" + notice_block
    else:
        new_description = notice_block

    if new_description.strip() != current_description.strip():
        pr.edit(body=new_description)
        print("Updated PR description with merge-queue evidence notice")


def ensure_merge_queue_notice_comment(pr: Any, status: str = "unknown") -> None:
    """Ensure the PR has a single bot-managed merge-queue evidence notice.

    See ``ensure_merge_queue_notice_description`` for what ``status`` means.
    """
    body = "\n".join([MERGE_QUEUE_COMMENT_MARKER] + _build_merge_queue_notice_lines(status))

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
