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

"""Create/update persistent GitHub issues for flaky test targets.

One issue is maintained per flaky Bazel test target. On each nightly run where a
target is flaky, a machine-readable run record is appended as an issue comment
(the durable, append-only ledger) and the issue body's managed stats region is
regenerated from the full comment ledger. Closed issues are reopened on
recurrence; issues are never auto-closed.

The module is split into a pure-logic core (no I/O) and an injectable GitHub
client, so the decision logic is fully unit-testable without network access.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import urllib.error
import urllib.request
from dataclasses import dataclass, field
from datetime import datetime, timezone
from pathlib import Path
from typing import Iterable, Protocol, Sequence

FLAKY_LABEL = "flaky-test"

TARGET_MARKER_RE = re.compile(r"<!--\s*flaky-test-target:\s*(?P<target>\S+)\s*-->")
RUN_MARKER_RE = re.compile(r"<!--\s*flaky-run:\s*(?P<json>\{.*?\})\s*-->", re.DOTALL)
STATS_BEGIN = "<!-- flaky-stats:begin -->"
STATS_END = "<!-- flaky-stats:end -->"
STATS_REGION_RE = re.compile(
    re.escape(STATS_BEGIN) + r".*?" + re.escape(STATS_END),
    re.DOTALL,
)


# --------------------------------------------------------------------------- #
# Data model
# --------------------------------------------------------------------------- #
@dataclass(frozen=True)
class RunRecord:
    """A single nightly observation of a target's flakiness."""

    run_id: str
    date: str
    failed_runs: int
    total_runs: int
    configs: dict[str, dict[str, int]]

    def to_marker(self) -> str:
        payload = {
            "run_id": self.run_id,
            "date": self.date,
            "failed_runs": self.failed_runs,
            "total_runs": self.total_runs,
            "configs": self.configs,
        }
        return f"<!-- flaky-run: {json.dumps(payload, sort_keys=True)} -->"


@dataclass
class Stats:
    """Cumulative statistics derived from the full run ledger."""

    target: str
    cumulative_failed_runs: int
    cumulative_total_runs: int
    nightly_count: int
    first_seen: str
    last_seen: str
    configs: dict[str, dict[str, int]]


@dataclass
class Issue:
    number: int
    body: str
    state: str = "open"  # "open" | "closed"
    labels: list[str] = field(default_factory=list)


@dataclass
class RunContext:
    run_id: str
    run_url: str
    date: str


# --------------------------------------------------------------------------- #
# GitHub client interface
# --------------------------------------------------------------------------- #
class GitHubClient(Protocol):
    def ensure_label(self, name: str) -> None: ...

    def search_issue(self, target: str) -> Issue | None: ...

    def list_run_comments(self, issue: Issue) -> list[str]: ...

    def create_issue(self, title: str, body: str, labels: list[str]) -> Issue: ...

    def update_issue_body(self, issue: Issue, body: str) -> None: ...

    def reopen_issue(self, issue: Issue) -> None: ...

    def add_comment(self, issue: Issue, body: str) -> None: ...


# --------------------------------------------------------------------------- #
# Pure functions (no I/O) — the tested core
# --------------------------------------------------------------------------- #
def parse_run_records(comment_bodies: Iterable[str]) -> list[RunRecord]:
    """Extract RunRecords from bot comment bodies via the flaky-run marker.

    Malformed or non-ledger comments are ignored. Duplicate run_ids are
    de-duplicated (first occurrence wins), making recomputation idempotent.
    """
    records: dict[str, RunRecord] = {}
    for body in comment_bodies:
        if not body:
            continue
        match = RUN_MARKER_RE.search(body)
        if not match:
            continue
        try:
            payload = json.loads(match.group("json"))
        except (json.JSONDecodeError, TypeError):
            continue
        run_id = str(payload.get("run_id", "")).strip()
        if not run_id or run_id in records:
            continue
        records[run_id] = RunRecord(
            run_id=run_id,
            date=str(payload.get("date", "")),
            failed_runs=int(payload.get("failed_runs", 0)),
            total_runs=int(payload.get("total_runs", 0)),
            configs={
                str(name): {
                    "failed_runs": int(vals.get("failed_runs", 0)),
                    "total_runs": int(vals.get("total_runs", 0)),
                }
                for name, vals in dict(payload.get("configs", {})).items()
            },
        )
    return list(records.values())


def aggregate(target: str, records: Sequence[RunRecord]) -> Stats:
    """Aggregate the full run ledger into cumulative Stats."""
    cumulative_failed = sum(r.failed_runs for r in records)
    cumulative_total = sum(r.total_runs for r in records)
    dates = sorted(r.date for r in records if r.date)
    configs: dict[str, dict[str, int]] = {}
    for record in records:
        for name, vals in record.configs.items():
            entry = configs.setdefault(name, {"failed_runs": 0, "total_runs": 0})
            entry["failed_runs"] += int(vals.get("failed_runs", 0))
            entry["total_runs"] += int(vals.get("total_runs", 0))
    return Stats(
        target=target,
        cumulative_failed_runs=cumulative_failed,
        cumulative_total_runs=cumulative_total,
        nightly_count=len(records),
        first_seen=dates[0] if dates else "",
        last_seen=dates[-1] if dates else "",
        configs=configs,
    )


def render_body(stats: Stats) -> str:
    """Render the full managed stats region (markers + counters + table + JSON)."""
    lines = [
        STATS_BEGIN,
        f"<!-- flaky-test-target: {stats.target} -->",
        f"## Flaky test: `{stats.target}`",
        "",
        (
            f"**Cumulative failed runs observed:** {stats.cumulative_failed_runs} "
            f"(over {stats.cumulative_total_runs} total runs, {stats.nightly_count} nightlies)"
        ),
        f"**First seen:** {stats.first_seen or 'n/a'} · **Last seen:** {stats.last_seen or 'n/a'}",
        "",
        "### Per-config cumulative",
        "",
        "| Config | Failed | Total |",
        "|--------|-------:|------:|",
    ]
    for name in sorted(stats.configs):
        vals = stats.configs[name]
        lines.append(f"| {name} | {vals['failed_runs']} | {vals['total_runs']} |")
    payload = {
        "target": stats.target,
        "cumulative_failed_runs": stats.cumulative_failed_runs,
        "cumulative_total_runs": stats.cumulative_total_runs,
        "nightly_count": stats.nightly_count,
        "first_seen": stats.first_seen,
        "last_seen": stats.last_seen,
        "configs": stats.configs,
    }
    lines.extend(
        [
            "",
            "```json",
            json.dumps(payload, indent=2, sort_keys=True),
            "```",
            STATS_END,
        ]
    )
    return "\n".join(lines)


def render_run_comment(record: RunRecord, run_url: str, reopened: bool = False) -> str:
    """Render a per-run ledger comment (human text + machine-readable marker)."""
    config_bits = ", ".join(
        f"{name} {vals['failed_runs']}/{vals['total_runs']}" for name, vals in sorted(record.configs.items())
    )
    lines = []
    if reopened:
        lines.append("♻️ Reopened: this test was flaky again after the issue was closed.")
        lines.append("")
    lines.extend(
        [
            f"Nightly run [`{record.run_id}`]({run_url}) on {record.date}: "
            f"**{record.failed_runs}/{record.total_runs}** runs failed.",
            "",
            f"Per config: {config_bits}." if config_bits else "",
            "",
            record.to_marker(),
        ]
    )
    return "\n".join(line for line in lines if line is not None)


def merge_body(existing_body: str, new_region: str) -> str:
    """Replace the managed stats region in an existing body, preserving prose.

    If the markers are absent (or corrupted such that a full region is missing),
    the fresh region is appended at the bottom so state self-heals.
    """
    existing_body = existing_body or ""
    if STATS_BEGIN in existing_body and STATS_END in existing_body:
        return STATS_REGION_RE.sub(lambda _m: new_region, existing_body, count=1)
    separator = "\n\n" if existing_body.strip() else ""
    return f"{existing_body.rstrip()}{separator}{new_region}\n" if existing_body.strip() else new_region + "\n"


def _run_record_from_target(target_item: dict, ctx: RunContext) -> RunRecord:
    return RunRecord(
        run_id=ctx.run_id,
        date=ctx.date,
        failed_runs=int(target_item.get("failed_runs", 0)),
        total_runs=int(target_item.get("total_runs", 0)),
        configs={
            str(name): {
                "failed_runs": int(vals.get("failed_runs", 0)),
                "total_runs": int(vals.get("total_runs", 0)),
            }
            for name, vals in dict(target_item.get("configs", {})).items()
        },
    )


def _issue_title(target: str) -> str:
    return f"Flaky test: {target}"


# --------------------------------------------------------------------------- #
# Orchestration
# --------------------------------------------------------------------------- #
def sync(merged_summary: dict, client: GitHubClient, ctx: RunContext) -> list[dict]:
    """Create/update flaky issues for all flaky targets in the merged summary.

    Returns a list of action records (for logging / dry-run visibility).
    """
    actions: list[dict] = []
    flaky_targets = merged_summary.get("flaky_targets", [])
    if not flaky_targets:
        return actions

    client.ensure_label(FLAKY_LABEL)

    for target_item in flaky_targets:
        target = target_item["target"]
        this_run = _run_record_from_target(target_item, ctx)
        existing = client.search_issue(target)

        if existing is None:
            # Create branch: create issue, seed the ledger, render body once.
            stats = aggregate(target, [this_run])
            body = render_body(stats)
            issue = client.create_issue(_issue_title(target), body, [FLAKY_LABEL])
            client.add_comment(issue, render_run_comment(this_run, ctx.run_url))
            actions.append({"target": target, "action": "created", "issue": issue.number})
            continue

        # Update branch.
        comments = client.list_run_comments(existing)
        existing_records = parse_run_records(comments)
        if any(r.run_id == ctx.run_id for r in existing_records):
            actions.append(
                {
                    "target": target,
                    "action": "skipped-duplicate",
                    "issue": existing.number,
                }
            )
            continue

        reopened = existing.state == "closed"
        if reopened:
            client.reopen_issue(existing)
        client.add_comment(existing, render_run_comment(this_run, ctx.run_url, reopened=reopened))

        all_records = existing_records + [this_run]
        stats = aggregate(target, all_records)
        client.update_issue_body(existing, merge_body(existing.body, render_body(stats)))
        actions.append(
            {
                "target": target,
                "action": "reopened" if reopened else "updated",
                "issue": existing.number,
            }
        )
    return actions


# --------------------------------------------------------------------------- #
# Real GitHub REST client
# --------------------------------------------------------------------------- #
class RestGitHubClient:
    """GitHub REST client using GITHUB_TOKEN. Follows pagination."""

    def __init__(self, repo: str, token: str, api_url: str = "https://api.github.com"):
        self._repo = repo
        self._token = token
        self._api = api_url.rstrip("/")

    def _request(self, method: str, path: str, payload: dict | None = None) -> object:
        url = path if path.startswith("http") else f"{self._api}{path}"
        data = json.dumps(payload).encode("utf-8") if payload is not None else None
        request = urllib.request.Request(url, data=data, method=method)
        request.add_header("Authorization", f"Bearer {self._token}")
        request.add_header("Accept", "application/vnd.github+json")
        request.add_header("X-GitHub-Api-Version", "2022-11-28")
        if data is not None:
            request.add_header("Content-Type", "application/json")
        with urllib.request.urlopen(request) as response:  # noqa: S310 (trusted API URL)
            body = response.read().decode("utf-8")
        return json.loads(body) if body else None

    def _paginate(self, path: str) -> list[dict]:
        results: list[dict] = []
        page = 1
        while True:
            sep = "&" if "?" in path else "?"
            chunk = self._request("GET", f"{path}{sep}per_page=100&page={page}")
            if not isinstance(chunk, list) or not chunk:
                break
            results.extend(chunk)
            if len(chunk) < 100:
                break
            page += 1
        return results

    def ensure_label(self, name: str) -> None:
        try:
            self._request("GET", f"/repos/{self._repo}/labels/{name}")
        except urllib.error.HTTPError as error:
            if error.code != 404:
                raise
            try:
                self._request(
                    "POST",
                    f"/repos/{self._repo}/labels",
                    {
                        "name": name,
                        "color": "d73a4a",
                        "description": "Detected flaky test target",
                    },
                )
            except urllib.error.HTTPError as create_error:
                if create_error.code != 422:  # already exists (race)
                    raise

    def search_issue(self, target: str) -> Issue | None:
        # Search open+closed issues carrying the label; match by body marker.
        issues = self._paginate(f"/repos/{self._repo}/issues?state=all&labels={FLAKY_LABEL}")
        for raw in issues:
            if "pull_request" in raw:
                continue
            body = raw.get("body") or ""
            marker = TARGET_MARKER_RE.search(body)
            if (marker and marker.group("target") == target) or (
                target in body and _issue_title(target) == (raw.get("title") or "")
            ):
                return Issue(
                    number=int(raw["number"]),
                    body=body,
                    state=str(raw.get("state", "open")),
                    labels=[lbl["name"] for lbl in raw.get("labels", [])],
                )
        return None

    def list_run_comments(self, issue: Issue) -> list[str]:
        comments = self._paginate(f"/repos/{self._repo}/issues/{issue.number}/comments")
        return [c.get("body") or "" for c in comments]

    def create_issue(self, title: str, body: str, labels: list[str]) -> Issue:
        raw = self._request(
            "POST",
            f"/repos/{self._repo}/issues",
            {"title": title, "body": body, "labels": labels},
        )
        assert isinstance(raw, dict)
        return Issue(number=int(raw["number"]), body=body, state="open", labels=labels)

    def update_issue_body(self, issue: Issue, body: str) -> None:
        self._request("PATCH", f"/repos/{self._repo}/issues/{issue.number}", {"body": body})
        issue.body = body

    def reopen_issue(self, issue: Issue) -> None:
        self._request("PATCH", f"/repos/{self._repo}/issues/{issue.number}", {"state": "open"})
        issue.state = "open"

    def add_comment(self, issue: Issue, body: str) -> None:
        self._request(
            "POST",
            f"/repos/{self._repo}/issues/{issue.number}/comments",
            {"body": body},
        )


# --------------------------------------------------------------------------- #
# Dry-run client (logs actions, no API calls)
# --------------------------------------------------------------------------- #
class DryRunGitHubClient:
    def __init__(self) -> None:
        self.log: list[str] = []

    def ensure_label(self, name: str) -> None:
        self.log.append(f"ensure_label({name})")

    def search_issue(self, target: str) -> Issue | None:
        self.log.append(f"search_issue({target}) -> None")
        return None

    def list_run_comments(self, issue: Issue) -> list[str]:
        return []

    def create_issue(self, title: str, body: str, labels: list[str]) -> Issue:
        self.log.append(f"create_issue(title={title!r}, labels={labels})")
        return Issue(number=0, body=body, state="open", labels=labels)

    def update_issue_body(self, issue: Issue, body: str) -> None:
        self.log.append(f"update_issue_body(#{issue.number})")

    def reopen_issue(self, issue: Issue) -> None:
        self.log.append(f"reopen_issue(#{issue.number})")

    def add_comment(self, issue: Issue, body: str) -> None:
        self.log.append(f"add_comment(#{issue.number})")


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Sync flaky test GitHub issues.")
    parser.add_argument("--merged-summary", required=True)
    parser.add_argument("--repo", required=True)
    parser.add_argument("--run-id", required=True)
    parser.add_argument("--run-url", required=True)
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args(argv)

    merged_summary = json.loads(Path(args.merged_summary).read_text(encoding="utf-8"))
    ctx = RunContext(
        run_id=str(args.run_id),
        run_url=args.run_url,
        date=datetime.now(timezone.utc).date().isoformat(),
    )

    if args.dry_run:
        client: GitHubClient = DryRunGitHubClient()
    else:
        token = os.environ.get("GITHUB_TOKEN", "")
        if not token:
            parser.error("GITHUB_TOKEN environment variable is required (or use --dry-run).")
        client = RestGitHubClient(args.repo, token)

    actions = sync(merged_summary, client, ctx)
    for action in actions:
        print(f"{action['action']}: {action['target']} (issue #{action.get('issue')})")
    if args.dry_run and isinstance(client, DryRunGitHubClient):
        for entry in client.log:
            print(f"[dry-run] {entry}")
    print(f"Synced {len(actions)} flaky target(s).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
