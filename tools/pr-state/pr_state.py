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

"""Label every open pull request with whose turn it is ("ball in court").

Exactly one of the labels below is set on every open, non-draft pull request:

* ``state:needs-author``     - the author has to act
* ``state:needs-maintainer`` - a maintainer has to act
* ``state:ready-to-merge``   - approved and green, ready to be merged

In addition ``needs-maintainer-overdue`` is set when a pull request has been
waiting for a maintainer for too long.

Only the standard library and the ``gh`` CLI are used. No code of a pull
request is ever executed or checked out.
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from dataclasses import dataclass, field
from datetime import datetime, timedelta, timezone
from typing import Any, Iterable, Optional

NEEDS_AUTHOR = "state:needs-author"
NEEDS_MAINTAINER = "state:needs-maintainer"
READY_TO_MERGE = "state:ready-to-merge"
STATE_LABELS = (NEEDS_AUTHOR, NEEDS_MAINTAINER, READY_TO_MERGE)
OVERDUE_LABEL = "needs-maintainer-overdue"
STALE_LABEL = "stale"
OVERDUE_AFTER = timedelta(days=7)

LABEL_DEFINITIONS = {
    NEEDS_AUTHOR: ("d93f0b", "Waiting for an action of the pull request author"),
    NEEDS_MAINTAINER: ("0e8a16", "Waiting for an action of a maintainer"),
    READY_TO_MERGE: ("1d76db", "Approved and all required checks are green"),
    OVERDUE_LABEL: ("b60205", "Waiting for a maintainer for more than 7 days"),
}

FAILED_CHECK_CONCLUSIONS = {"FAILURE", "TIMED_OUT", "STARTUP_FAILURE", "ACTION_REQUIRED"}
GREEN_CHECK_CONCLUSIONS = {"SUCCESS", "NEUTRAL", "SKIPPED"}
FAILED_STATUS_STATES = {"FAILURE", "ERROR"}

PR_QUERY = """
query($owner: String!, $name: String!, $number: Int!) {
  repository(owner: $owner, name: $name) {
    pullRequest(number: $number) {
      number
      title
      url
      state
      isDraft
      mergeable
      reviewDecision
      author { login }
      labels(first: 50) { nodes { name } }
      commits(last: 1) {
        nodes {
          commit {
            committedDate
            statusCheckRollup {
              contexts(first: 100) {
                nodes {
                  __typename
                  ... on CheckRun {
                    status
                    conclusion
                    isRequired(pullRequestNumber: $number)
                  }
                  ... on StatusContext {
                    state
                    isRequired(pullRequestNumber: $number)
                  }
                }
              }
            }
          }
        }
      }
      comments(last: 50) { nodes { author { login } createdAt } }
      reviews(last: 50) { nodes { author { __typename login } state submittedAt } }
      reviewThreads(last: 50) {
        nodes { comments(last: 1) { nodes { author { login } createdAt } } }
      }
      timelineItems(last: 100, itemTypes: [LABELED_EVENT, REVIEW_REQUESTED_EVENT]) {
        nodes {
          __typename
          ... on LabeledEvent { createdAt label { name } }
          ... on ReviewRequestedEvent { createdAt }
        }
      }
    }
  }
}
"""


@dataclass(frozen=True)
class Snapshot:
    """Everything needed to decide whose turn it is, free of any I/O."""

    number: int
    title: str = ""
    url: str = ""
    author: str = ""
    is_draft: bool = False
    mergeable: str = "UNKNOWN"
    review_decision: Optional[str] = None
    required_checks_failed: bool = False
    required_checks_green: bool = True
    changes_requested_at: Optional[datetime] = None
    author_activity_at: Optional[datetime] = None
    labels: frozenset[str] = field(default_factory=frozenset)
    state_since: Optional[datetime] = None


def compute_state(snapshot: Snapshot) -> Optional[str]:
    """Return the state label for a pull request, None for drafts."""
    if snapshot.is_draft:
        return None

    if snapshot.mergeable == "CONFLICTING" or snapshot.required_checks_failed:
        return NEEDS_AUTHOR

    if snapshot.review_decision == "CHANGES_REQUESTED":
        answered = (
            snapshot.changes_requested_at is not None
            and snapshot.author_activity_at is not None
            and snapshot.author_activity_at > snapshot.changes_requested_at
        )
        return NEEDS_MAINTAINER if answered else NEEDS_AUTHOR

    if snapshot.review_decision == "APPROVED" and snapshot.required_checks_green:
        return READY_TO_MERGE

    return NEEDS_MAINTAINER


def compute_labels(snapshot: Snapshot, now: datetime) -> frozenset[str]:
    """Return the desired set of managed labels (state, overdue) for a pull request."""
    state = compute_state(snapshot)
    if state is None:
        return frozenset()
    labels = {state}
    if (
        state == NEEDS_MAINTAINER
        and NEEDS_MAINTAINER in snapshot.labels
        and snapshot.state_since is not None
        and now - snapshot.state_since > OVERDUE_AFTER
    ):
        labels.add(OVERDUE_LABEL)
    return frozenset(labels)


@dataclass(frozen=True)
class LabelChange:
    add: frozenset[str]
    remove: frozenset[str]

    def __bool__(self) -> bool:
        return bool(self.add or self.remove)


def compute_label_change(snapshot: Snapshot, now: datetime) -> LabelChange:
    """Diff the desired managed labels against the labels currently set."""
    desired = compute_labels(snapshot, now)
    managed = set(STATE_LABELS) | {OVERDUE_LABEL}
    current = snapshot.labels & managed
    add = set(desired - current)
    remove = set(current - desired)

    # A state flip means someone acted, so a previous stale marker is outdated.
    state_changed = (desired & set(STATE_LABELS)) != (current & set(STATE_LABELS))
    if state_changed and STALE_LABEL in snapshot.labels:
        remove.add(STALE_LABEL)
    return LabelChange(frozenset(add), frozenset(remove))


def _parse_time(value: Optional[str]) -> Optional[datetime]:
    if not value:
        return None
    return datetime.fromisoformat(value.replace("Z", "+00:00"))


def _login(node: Optional[dict[str, Any]]) -> str:
    return ((node or {}).get("author") or {}).get("login", "")


def _latest(times: Iterable[Optional[datetime]]) -> Optional[datetime]:
    present = [t for t in times if t is not None]
    return max(present) if present else None


def parse_snapshot(data: dict[str, Any]) -> Snapshot:
    """Convert the ``pullRequest`` GraphQL object into a Snapshot."""
    author = (data.get("author") or {}).get("login", "")
    labels = frozenset(n["name"] for n in data["labels"]["nodes"])

    commit = (data["commits"]["nodes"] or [{}])[0].get("commit", {})
    rollup = commit.get("statusCheckRollup") or {}
    required = [c for c in (rollup.get("contexts") or {}).get("nodes", []) if c and c.get("isRequired")]
    failed = green = 0
    for ctx in required:
        if ctx["__typename"] == "CheckRun":
            if ctx.get("conclusion") in FAILED_CHECK_CONCLUSIONS:
                failed += 1
            elif ctx.get("conclusion") in GREEN_CHECK_CONCLUSIONS:
                green += 1
        elif ctx.get("state") in FAILED_STATUS_STATES:
            failed += 1
        elif ctx.get("state") == "SUCCESS":
            green += 1

    human_reviews = [
        r
        for r in data["reviews"]["nodes"]
        if r["author"] and r["author"]["login"] != author and r["author"]["__typename"] != "Bot"
    ]
    changes_requested_at = _latest(
        _parse_time(r["submittedAt"]) for r in human_reviews if r["state"] == "CHANGES_REQUESTED"
    )

    thread_replies = [c for t in data["reviewThreads"]["nodes"] for c in t["comments"]["nodes"]]
    author_events = [
        _parse_time(commit.get("committedDate")),
        *(_parse_time(c["createdAt"]) for c in data["comments"]["nodes"] if _login(c) == author),
        *(_parse_time(c["createdAt"]) for c in thread_replies if _login(c) == author),
        *(
            _parse_time(r["submittedAt"])
            for r in data["reviews"]["nodes"]
            if r["author"] and r["author"]["login"] == author
        ),
        *(
            _parse_time(t["createdAt"])
            for t in data["timelineItems"]["nodes"]
            if t["__typename"] == "ReviewRequestedEvent"
        ),
    ]

    state_since = _latest(
        _parse_time(t["createdAt"])
        for t in data["timelineItems"]["nodes"]
        if t["__typename"] == "LabeledEvent" and t["label"]["name"] in STATE_LABELS and t["label"]["name"] in labels
    )

    return Snapshot(
        number=data["number"],
        title=data["title"],
        url=data["url"],
        author=author,
        is_draft=data["isDraft"],
        mergeable=data["mergeable"],
        review_decision=data["reviewDecision"],
        required_checks_failed=failed > 0,
        required_checks_green=failed == 0 and green == len(required),
        changes_requested_at=changes_requested_at,
        author_activity_at=_latest(author_events),
        labels=labels,
        state_since=state_since,
    )


def _gh(*args: str) -> str:
    result = subprocess.run(["gh", *args], capture_output=True, text=True, check=True)
    return result.stdout


def fetch_snapshot(repo: str, number: int) -> Optional[Snapshot]:
    """Fetch a snapshot, None if the pull request is not open."""
    owner, name = repo.split("/")
    out = _gh(
        "api", "graphql",
        "-f", f"query={PR_QUERY}",
        "-F", f"owner={owner}",
        "-F", f"name={name}",
        "-F", f"number={number}",
    )  # fmt: skip
    data = json.loads(out)["data"]["repository"]["pullRequest"]
    if data["state"] != "OPEN":
        return None
    return parse_snapshot(data)


def list_open_pull_requests(repo: str) -> list[int]:
    out = _gh("pr", "list", "--repo", repo, "--state", "open", "--limit", "1000", "--json", "number")
    return [pr["number"] for pr in json.loads(out)]


def ensure_labels(repo: str) -> None:
    for name, (color, description) in LABEL_DEFINITIONS.items():
        _gh("label", "create", name, "--repo", repo, "--color", color,
            "--description", description, "--force")  # fmt: skip


def apply_change(repo: str, number: int, change: LabelChange) -> None:
    args = ["pr", "edit", str(number), "--repo", repo]
    for label in sorted(change.add):
        args += ["--add-label", label]
    for label in sorted(change.remove):
        args += ["--remove-label", label]
    _gh(*args)


def process(repo: str, number: int, dry_run: bool, now: datetime) -> None:
    snapshot = fetch_snapshot(repo, number)
    if snapshot is None:
        print(f"#{number}: not open, skipped")
        return
    change = compute_label_change(snapshot, now)
    if not change:
        print(f"#{number}: up to date")
        return
    print(f"#{number}: add={sorted(change.add)} remove={sorted(change.remove)}")
    if not dry_run:
        apply_change(repo, number, change)


def main(argv: Optional[list[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--repo", required=True, help="owner/name")
    parser.add_argument("--pr", type=int, help="single pull request number; default: all open")
    parser.add_argument("--dry-run", action="store_true", help="only log intended changes")
    args = parser.parse_args(argv)

    now = datetime.now(timezone.utc)
    if not args.dry_run:
        ensure_labels(args.repo)
    numbers = [args.pr] if args.pr else list_open_pull_requests(args.repo)
    failures = 0
    for number in numbers:
        try:
            process(args.repo, number, args.dry_run, now)
        except (subprocess.CalledProcessError, KeyError, ValueError) as error:
            failures += 1
            print(f"#{number}: failed: {error}", file=sys.stderr)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
