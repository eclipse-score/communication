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

"""Publish an overview of pull requests that need an action into one issue.

The issue titled ``PR overview`` is created on first use (and pinned) and is
edited afterwards, so that no notifications are produced by the updates.
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from datetime import datetime, timedelta, timezone
from typing import Optional

from pr_state import (
    NEEDS_AUTHOR,
    NEEDS_MAINTAINER,
    OVERDUE_LABEL,
    Snapshot,
    _gh,
    fetch_snapshot,
    list_open_pull_requests,
)

ISSUE_TITLE = "PR overview"
STALE_AFTER = timedelta(days=14)
STALE_WARNING = timedelta(days=3)


def _age(since: Optional[datetime], now: datetime) -> str:
    if since is None:
        return "?"
    return f"{(now - since).days}d"


def _row(snapshot: Snapshot, now: datetime) -> str:
    title = snapshot.title.replace("|", "\\|")
    waiting = _age(snapshot.state_since, now)
    if OVERDUE_LABEL in snapshot.labels:
        waiting += " (overdue)"
    return f"| #{snapshot.number} | {title} | @{snapshot.author} | {waiting} |"


def render(snapshots: list[Snapshot], now: datetime) -> str:
    maintainer = sorted(
        (s for s in snapshots if NEEDS_MAINTAINER in s.labels),
        key=lambda s: s.state_since or now,
    )
    near_stale = sorted(
        (
            s
            for s in snapshots
            if NEEDS_AUTHOR in s.labels
            and s.state_since is not None
            and now - s.state_since > STALE_AFTER - STALE_WARNING
        ),
        key=lambda s: s.state_since or now,
    )
    header = "| PR | Title | Author | Waiting |\n|---|---|---|---|"
    lines = [
        f"_Updated {now:%Y-%m-%d %H:%M} UTC by the PR digest workflow. Do not edit._",
        "",
        f"## Waiting for a maintainer ({len(maintainer)})",
        "",
        header,
        *(_row(s, now) for s in maintainer),
        "",
        f"## Waiting for the author, close to becoming stale ({len(near_stale)})",
        "",
        header,
        *(_row(s, now) for s in near_stale),
    ]
    return "\n".join(lines) + "\n"


def find_issue(repo: str) -> Optional[int]:
    out = _gh(
        "issue", "list", "--repo", repo, "--state", "open", "--author", "app/github-actions",
        "--search", f'"{ISSUE_TITLE}" in:title', "--json", "number,title",
    )  # fmt: skip
    for issue in json.loads(out):
        if issue["title"] == ISSUE_TITLE:
            return issue["number"]
    return None


def publish(repo: str, body: str) -> None:
    number = find_issue(repo)
    if number is not None:
        _gh("issue", "edit", str(number), "--repo", repo, "--body", body)
        return
    url = _gh("issue", "create", "--repo", repo, "--title", ISSUE_TITLE, "--body", body).strip()
    number = int(url.rsplit("/", 1)[1])
    node_id = json.loads(_gh("issue", "view", str(number), "--repo", repo, "--json", "id"))["id"]
    try:
        _gh("api", "graphql", "-f", "query=mutation($id: ID!) { pinIssue(input: {issueId: $id}) { issue { id } } }",
            "-f", f"id={node_id}")  # fmt: skip
    except subprocess.CalledProcessError as error:
        print(f"Could not pin issue #{number}: {error.stderr}", file=sys.stderr)


def main(argv: Optional[list[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--repo", required=True, help="owner/name")
    parser.add_argument("--dry-run", action="store_true", help="print instead of publishing")
    args = parser.parse_args(argv)

    now = datetime.now(timezone.utc)
    snapshots = [s for s in (fetch_snapshot(args.repo, n) for n in list_open_pull_requests(args.repo)) if s is not None]
    body = render(snapshots, now)
    if args.dry_run:
        print(body)
    else:
        publish(args.repo, body)
    return 0


if __name__ == "__main__":
    sys.exit(main())
