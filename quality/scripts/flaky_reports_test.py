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

import json
import os
import tempfile
import unittest

from quality.scripts import collect_flaky_tests, merge_flaky_reports, sync_flaky_issues
from quality.scripts.sync_flaky_issues import (
    Issue,
    RunContext,
    RunRecord,
    aggregate,
    merge_body,
    parse_run_records,
    render_body,
    sync,
)


class CollectFlakyTest(unittest.TestCase):
    def test_collect_reports_flaky_and_failed_targets(self):
        with tempfile.TemporaryDirectory() as tmp:
            bep_path = os.path.join(tmp, "bep.json")
            raw_path = os.path.join(tmp, "raw.log")
            out_dir = os.path.join(tmp, "out")
            gh_out = os.path.join(tmp, "gh.out")

            with open(bep_path, "w", encoding="utf-8") as stream:
                stream.write(
                    json.dumps(
                        {
                            "id": {"testSummary": {"label": "//pkg:flaky_low"}},
                            "testSummary": {"overallStatus": "FLAKY", "failed": [{}] * 5, "passed": [{}] * 995},
                        }
                    )
                    + "\n"
                )
                stream.write(
                    json.dumps(
                        {
                            "id": {"testSummary": {"label": "//pkg:flaky_high"}},
                            "testSummary": {"overallStatus": "FLAKY", "failed": [{}] * 11, "passed": [{}] * 989},
                        }
                    )
                    + "\n"
                )
                stream.write(
                    json.dumps(
                        {
                            "id": {"testSummary": {"label": "//pkg:failed"}},
                            "testSummary": {"overallStatus": "FAILED", "failed": [{}], "passed": []},
                        }
                    )
                    + "\n"
                )
            with open(raw_path, "w", encoding="utf-8") as stream:
                stream.write("//pkg:passed PASSED in 0.2s\n")

            exit_code = collect_flaky_tests.main(
                [
                    "--config-name",
                    "gcc15",
                    "--bep-json",
                    bep_path,
                    "--raw-log",
                    raw_path,
                    "--output-dir",
                    out_dir,
                    "--test-exit-code",
                    "1",
                    "--runs-per-test",
                    "1000",
                    "--github-output",
                    gh_out,
                ]
            )
            self.assertEqual(exit_code, 0)

            with open(os.path.join(out_dir, "summary.json"), encoding="utf-8") as stream:
                summary = json.load(stream)
            self.assertEqual(summary["flaky_count"], 2)
            self.assertEqual(summary["failed_count"], 1)
            self.assertEqual(summary["passed_count"], 1)
            self.assertEqual(summary["flaky_tests"], ["//pkg:flaky_high", "//pkg:flaky_low"])
            self.assertEqual(summary["failed_tests"], ["//pkg:failed"])
            self.assertNotIn("accepted_flaky_tests", summary)
            self.assertNotIn("non_acceptable_flaky_count", summary)

            with open(gh_out, encoding="utf-8") as stream:
                output_text = stream.read()
            self.assertIn("flaky_count=2", output_text)
            self.assertIn("failed_count=1", output_text)
            self.assertNotIn("accepted_flaky_count", output_text)


class MergeFlakyTest(unittest.TestCase):
    def test_merge_aggregates_targets_across_configs(self):
        with tempfile.TemporaryDirectory() as tmp:
            reports_root = os.path.join(tmp, "reports")
            os.makedirs(os.path.join(reports_root, "a"), exist_ok=True)
            os.makedirs(os.path.join(reports_root, "b"), exist_ok=True)
            out_json = os.path.join(tmp, "merged", "summary.json")
            out_md = os.path.join(tmp, "merged", "summary.md")
            gh_out = os.path.join(tmp, "gh.out")

            with open(os.path.join(reports_root, "a", "summary.json"), "w", encoding="utf-8") as stream:
                json.dump(
                    {
                        "config_name": "tsan",
                        "flaky_count": 1,
                        "flaky_test_details": [
                            {"target": "//pkg:shared", "failed_runs": 7, "total_runs": 300, "failures_per_thousand": 23.3}
                        ],
                        "failed_count": 0,
                        "passed_count": 10,
                        "test_exit_code": 1,
                    },
                    stream,
                )
            with open(os.path.join(reports_root, "b", "summary.json"), "w", encoding="utf-8") as stream:
                json.dump(
                    {
                        "config_name": "asan",
                        "flaky_count": 1,
                        "flaky_test_details": [
                            {"target": "//pkg:shared", "failed_runs": 5, "total_runs": 300, "failures_per_thousand": 16.6}
                        ],
                        "failed_count": 2,
                        "passed_count": 5,
                        "test_exit_code": 2,
                    },
                    stream,
                )

            exit_code = merge_flaky_reports.main(
                [
                    "--reports-root",
                    reports_root,
                    "--output-json",
                    out_json,
                    "--output-md",
                    out_md,
                    "--github-output",
                    gh_out,
                ]
            )
            self.assertEqual(exit_code, 0)

            with open(out_json, encoding="utf-8") as stream:
                merged = json.load(stream)
            self.assertEqual(merged["total_flaky_count"], 1)
            self.assertEqual(merged["total_failed_count"], 2)
            self.assertEqual(len(merged["flaky_targets"]), 1)
            target = merged["flaky_targets"][0]
            self.assertEqual(target["target"], "//pkg:shared")
            self.assertEqual(target["failed_runs"], 12)
            self.assertEqual(target["total_runs"], 600)
            self.assertEqual(set(target["configs"]), {"tsan", "asan"})
            self.assertEqual(target["configs"]["tsan"]["failed_runs"], 7)

            with open(gh_out, encoding="utf-8") as stream:
                output_text = stream.read()
            self.assertIn("total_flaky_count=1", output_text)
            self.assertIn("total_failed_count=2", output_text)


class FakeGitHubClient:
    """In-memory GitHub client for testing sync orchestration."""

    def __init__(self):
        self.issues = {}
        self.comments = {}
        self._next_number = 1
        self.labels_ensured = []
        self.reopened = []

    def ensure_label(self, name):
        self.labels_ensured.append(name)

    def _find(self, target):
        for issue in self.issues.values():
            marker = sync_flaky_issues.TARGET_MARKER_RE.search(issue.body or "")
            if marker and marker.group("target") == target:
                return issue
        return None

    def search_issue(self, target):
        return self._find(target)

    def list_run_comments(self, issue):
        return list(self.comments.get(issue.number, []))

    def create_issue(self, title, body, labels):
        number = self._next_number
        self._next_number += 1
        issue = Issue(number=number, body=body, state="open", labels=list(labels))
        self.issues[number] = issue
        self.comments[number] = []
        return issue

    def update_issue_body(self, issue, body):
        self.issues[issue.number].body = body
        issue.body = body

    def reopen_issue(self, issue):
        self.issues[issue.number].state = "open"
        issue.state = "open"
        self.reopened.append(issue.number)

    def add_comment(self, issue, body):
        self.comments.setdefault(issue.number, []).append(body)


def _summary(target, failed, total, configs):
    return {
        "flaky_targets": [
            {"target": target, "failed_runs": failed, "total_runs": total, "configs": configs}
        ]
    }


class SyncFlakyIssuesTest(unittest.TestCase):
    def _ctx(self, run_id="100"):
        return RunContext(run_id=run_id, run_url=f"https://ci/{run_id}", date="2026-07-27")

    def test_new_target_creates_issue_with_single_comment(self):
        client = FakeGitHubClient()
        summary = _summary("//pkg:a", 7, 300, {"tsan": {"failed_runs": 7, "total_runs": 300}})
        actions = sync(summary, client, self._ctx())

        self.assertEqual(actions[0]["action"], "created")
        self.assertEqual(len(client.issues), 1)
        issue = client.issues[1]
        self.assertIn("flaky-test", issue.labels)
        self.assertIn("<!-- flaky-test-target: //pkg:a -->", issue.body)
        # Exactly one run comment written on creation.
        self.assertEqual(len(client.comments[1]), 1)
        self.assertIn("Cumulative failed runs observed:** 7", issue.body)

    def test_recurrence_increments_counter_without_duplicate_issue(self):
        client = FakeGitHubClient()
        cfg = {"tsan": {"failed_runs": 7, "total_runs": 300}}
        sync(_summary("//pkg:a", 7, 300, cfg), client, self._ctx("100"))
        sync(_summary("//pkg:a", 4, 300, cfg), client, self._ctx("101"))

        self.assertEqual(len(client.issues), 1)
        self.assertEqual(len(client.comments[1]), 2)
        issue = client.issues[1]
        self.assertIn("Cumulative failed runs observed:** 11", issue.body)
        self.assertIn("over 600 total runs, 2 nightlies", issue.body)

    def test_same_run_id_is_idempotent(self):
        client = FakeGitHubClient()
        cfg = {"tsan": {"failed_runs": 7, "total_runs": 300}}
        sync(_summary("//pkg:a", 7, 300, cfg), client, self._ctx("100"))
        result = sync(_summary("//pkg:a", 7, 300, cfg), client, self._ctx("100"))

        self.assertEqual(result[0]["action"], "skipped-duplicate")
        self.assertEqual(len(client.comments[1]), 1)
        self.assertIn("Cumulative failed runs observed:** 7", client.issues[1].body)

    def test_closed_issue_is_reopened_on_recurrence(self):
        client = FakeGitHubClient()
        cfg = {"tsan": {"failed_runs": 7, "total_runs": 300}}
        sync(_summary("//pkg:a", 7, 300, cfg), client, self._ctx("100"))
        client.issues[1].state = "closed"

        result = sync(_summary("//pkg:a", 3, 300, cfg), client, self._ctx("101"))

        self.assertEqual(result[0]["action"], "reopened")
        self.assertIn(1, client.reopened)
        self.assertEqual(client.issues[1].state, "open")
        self.assertIn("Reopened", client.comments[1][-1])

    def test_corrupted_body_block_recovers_from_comments(self):
        client = FakeGitHubClient()
        cfg = {"tsan": {"failed_runs": 7, "total_runs": 300}}
        sync(_summary("//pkg:a", 7, 300, cfg), client, self._ctx("100"))
        # A human corrupts the managed region but keeps the identity marker.
        client.issues[1].body = "<!-- flaky-test-target: //pkg:a -->\nhuman notes, block deleted"

        sync(_summary("//pkg:a", 4, 300, cfg), client, self._ctx("101"))

        issue = client.issues[1]
        self.assertIn("human notes, block deleted", issue.body)
        self.assertIn("Cumulative failed runs observed:** 11", issue.body)


class SyncPureFunctionsTest(unittest.TestCase):
    def test_parse_run_records_ignores_malformed(self):
        good = RunRecord("1", "2026-07-27", 3, 300, {"tsan": {"failed_runs": 3, "total_runs": 300}}).to_marker()
        bodies = [good, "just a human comment", "<!-- flaky-run: {not json} -->"]
        records = parse_run_records(bodies)
        self.assertEqual(len(records), 1)
        self.assertEqual(records[0].failed_runs, 3)

    def test_aggregate_sums_across_configs(self):
        records = [
            RunRecord("1", "2026-07-20", 3, 300, {"tsan": {"failed_runs": 3, "total_runs": 300}}),
            RunRecord("2", "2026-07-27", 5, 300, {"asan": {"failed_runs": 5, "total_runs": 300}}),
        ]
        stats = aggregate("//pkg:a", records)
        self.assertEqual(stats.cumulative_failed_runs, 8)
        self.assertEqual(stats.cumulative_total_runs, 600)
        self.assertEqual(stats.nightly_count, 2)
        self.assertEqual(stats.first_seen, "2026-07-20")
        self.assertEqual(stats.last_seen, "2026-07-27")

    def test_merge_body_preserves_prose_outside_region(self):
        stats = aggregate("//pkg:a", [RunRecord("1", "2026-07-27", 1, 10, {})])
        region = render_body(stats)
        existing = f"Human triage notes.\n\n{region}\n\nMore notes below."
        # New stats after a second run.
        stats2 = aggregate(
            "//pkg:a",
            [RunRecord("1", "2026-07-27", 1, 10, {}), RunRecord("2", "2026-07-28", 2, 10, {})],
        )
        merged = merge_body(existing, render_body(stats2))
        self.assertIn("Human triage notes.", merged)
        self.assertIn("More notes below.", merged)
        self.assertIn("Cumulative failed runs observed:** 3", merged)
        self.assertEqual(merged.count(sync_flaky_issues.STATS_BEGIN), 1)


if __name__ == "__main__":
    unittest.main()
