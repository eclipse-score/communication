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

"""Tests for pr_state.py and pr_digest.py."""

import unittest
from datetime import datetime, timedelta, timezone

import pr_digest
from pr_state import (
    NEEDS_AUTHOR,
    NEEDS_MAINTAINER,
    OVERDUE_LABEL,
    READY_TO_MERGE,
    Snapshot,
    compute_label_change,
    compute_state,
    parse_snapshot,
)

NOW = datetime(2026, 11, 13, 12, 0, tzinfo=timezone.utc)
EARLIER = NOW - timedelta(days=2)
LATER = NOW - timedelta(days=1)


def snap(**kwargs) -> Snapshot:
    return Snapshot(number=1, author="alice", **kwargs)


class ComputeStateTest(unittest.TestCase):
    def test_draft_has_no_state(self):
        self.assertIsNone(compute_state(snap(is_draft=True, mergeable="CONFLICTING")))

    def test_new_pr_needs_maintainer(self):
        self.assertEqual(compute_state(snap()), NEEDS_MAINTAINER)

    def test_conflict_needs_author(self):
        self.assertEqual(compute_state(snap(mergeable="CONFLICTING")), NEEDS_AUTHOR)

    def test_failed_required_check_needs_author(self):
        self.assertEqual(
            compute_state(snap(required_checks_failed=True, review_decision="APPROVED")),
            NEEDS_AUTHOR,
        )

    def test_changes_requested_needs_author(self):
        s = snap(review_decision="CHANGES_REQUESTED", changes_requested_at=EARLIER)
        self.assertEqual(compute_state(s), NEEDS_AUTHOR)

    def test_author_push_after_changes_requested_needs_maintainer(self):
        s = snap(
            review_decision="CHANGES_REQUESTED",
            changes_requested_at=EARLIER,
            author_activity_at=LATER,
        )
        self.assertEqual(compute_state(s), NEEDS_MAINTAINER)

    def test_author_activity_before_changes_requested_needs_author(self):
        s = snap(
            review_decision="CHANGES_REQUESTED",
            changes_requested_at=LATER,
            author_activity_at=EARLIER,
        )
        self.assertEqual(compute_state(s), NEEDS_AUTHOR)

    def test_approved_and_green_is_ready(self):
        self.assertEqual(compute_state(snap(review_decision="APPROVED")), READY_TO_MERGE)

    def test_approved_with_pending_checks_needs_maintainer(self):
        s = snap(review_decision="APPROVED", required_checks_green=False)
        self.assertEqual(compute_state(s), NEEDS_MAINTAINER)


class LabelChangeTest(unittest.TestCase):
    def test_labels_new_pr(self):
        change = compute_label_change(snap(), NOW)
        self.assertEqual(change.add, {NEEDS_MAINTAINER})
        self.assertFalse(change.remove)

    def test_up_to_date_is_empty(self):
        self.assertFalse(compute_label_change(snap(labels=frozenset({NEEDS_MAINTAINER})), NOW))

    def test_swaps_state_label_and_ignores_foreign_labels(self):
        s = snap(mergeable="CONFLICTING", labels=frozenset({NEEDS_MAINTAINER, "bug"}))
        change = compute_label_change(s, NOW)
        self.assertEqual(change.add, {NEEDS_AUTHOR})
        self.assertEqual(change.remove, {NEEDS_MAINTAINER})

    def test_draft_loses_managed_labels(self):
        s = snap(is_draft=True, labels=frozenset({NEEDS_AUTHOR, OVERDUE_LABEL}))
        self.assertEqual(compute_label_change(s, NOW).remove, {NEEDS_AUTHOR, OVERDUE_LABEL})

    def test_overdue_after_seven_days(self):
        s = snap(
            labels=frozenset({NEEDS_MAINTAINER}),
            state_since=NOW - timedelta(days=8),
        )
        self.assertEqual(compute_label_change(s, NOW).add, {OVERDUE_LABEL})

    def test_not_overdue_before_seven_days(self):
        s = snap(
            labels=frozenset({NEEDS_MAINTAINER}),
            state_since=NOW - timedelta(days=6),
        )
        self.assertFalse(compute_label_change(s, NOW))

    def test_overdue_removed_when_state_flips(self):
        s = snap(
            mergeable="CONFLICTING",
            labels=frozenset({NEEDS_MAINTAINER, OVERDUE_LABEL}),
        )
        self.assertEqual(compute_label_change(s, NOW).remove, {NEEDS_MAINTAINER, OVERDUE_LABEL})

    def test_stale_removed_on_state_flip_only(self):
        flipped = snap(labels=frozenset({NEEDS_AUTHOR, "stale"}))
        self.assertIn("stale", compute_label_change(flipped, NOW).remove)
        unchanged = snap(mergeable="CONFLICTING", labels=frozenset({NEEDS_AUTHOR, "stale"}))
        self.assertFalse(compute_label_change(unchanged, NOW))


def graphql_pr(**overrides):
    data = {
        "number": 7,
        "title": "Add feature",
        "url": "https://github.com/o/r/pull/7",
        "state": "OPEN",
        "isDraft": False,
        "mergeable": "MERGEABLE",
        "reviewDecision": "CHANGES_REQUESTED",
        "author": {"login": "alice"},
        "labels": {"nodes": [{"name": NEEDS_AUTHOR}]},
        "commits": {
            "nodes": [
                {
                    "commit": {
                        "committedDate": "2026-11-10T10:00:00Z",
                        "statusCheckRollup": {
                            "contexts": {
                                "nodes": [
                                    {"__typename": "CheckRun", "conclusion": "SUCCESS", "isRequired": True},
                                    {"__typename": "CheckRun", "conclusion": "FAILURE", "isRequired": False},
                                    {"__typename": "StatusContext", "state": "SUCCESS", "isRequired": True},
                                ]
                            }
                        },
                    }
                }
            ]
        },
        "comments": {"nodes": [{"author": {"login": "bob"}, "createdAt": "2026-11-12T10:00:00Z"}]},
        "reviews": {
            "nodes": [
                {
                    "author": {"__typename": "User", "login": "bob"},
                    "state": "CHANGES_REQUESTED",
                    "submittedAt": "2026-11-11T10:00:00Z",
                },
                {
                    "author": {"__typename": "Bot", "login": "copilot"},
                    "state": "CHANGES_REQUESTED",
                    "submittedAt": "2026-11-12T10:00:00Z",
                },
            ]
        },
        "reviewThreads": {"nodes": []},
        "timelineItems": {
            "nodes": [
                {"__typename": "LabeledEvent", "createdAt": "2026-11-11T11:00:00Z", "label": {"name": NEEDS_AUTHOR}},
                {"__typename": "LabeledEvent", "createdAt": "2026-11-01T11:00:00Z", "label": {"name": "stale"}},
            ]
        },
    }
    data.update(overrides)
    return data


class ParseSnapshotTest(unittest.TestCase):
    def test_parses_graphql_response(self):
        s = parse_snapshot(graphql_pr())
        self.assertEqual(s.author, "alice")
        self.assertFalse(s.required_checks_failed)
        self.assertTrue(s.required_checks_green)
        self.assertEqual(s.changes_requested_at, datetime(2026, 11, 11, 10, tzinfo=timezone.utc))
        self.assertEqual(s.author_activity_at, datetime(2026, 11, 10, 10, tzinfo=timezone.utc))
        self.assertEqual(s.state_since, datetime(2026, 11, 11, 11, tzinfo=timezone.utc))
        self.assertEqual(compute_state(s), NEEDS_AUTHOR)

    def test_non_required_failure_is_ignored_but_required_failure_counts(self):
        data = graphql_pr()
        data["commits"]["nodes"][0]["commit"]["statusCheckRollup"]["contexts"]["nodes"][1]["isRequired"] = True
        s = parse_snapshot(data)
        self.assertTrue(s.required_checks_failed)
        self.assertFalse(s.required_checks_green)

    def test_pending_required_check_is_not_green(self):
        data = graphql_pr()
        data["commits"]["nodes"][0]["commit"]["statusCheckRollup"]["contexts"]["nodes"][0]["conclusion"] = None
        s = parse_snapshot(data)
        self.assertFalse(s.required_checks_failed)
        self.assertFalse(s.required_checks_green)

    def test_author_comment_counts_as_activity(self):
        data = graphql_pr()
        data["comments"]["nodes"].append({"author": {"login": "alice"}, "createdAt": "2026-11-12T12:00:00Z"})
        s = parse_snapshot(data)
        self.assertEqual(compute_state(s), NEEDS_MAINTAINER)

    def test_missing_rollup_means_no_required_checks(self):
        data = graphql_pr()
        data["commits"]["nodes"][0]["commit"]["statusCheckRollup"] = None
        s = parse_snapshot(data)
        self.assertTrue(s.required_checks_green)


class DigestTest(unittest.TestCase):
    def test_render_lists_oldest_first_and_flags_overdue(self):
        old = Snapshot(
            number=1, title="old | pipe", author="a",
            labels=frozenset({NEEDS_MAINTAINER, OVERDUE_LABEL}),
            state_since=NOW - timedelta(days=9),
        )  # fmt: skip
        new = Snapshot(
            number=2, title="new", author="b",
            labels=frozenset({NEEDS_MAINTAINER}),
            state_since=NOW - timedelta(days=1),
        )  # fmt: skip
        close_to_stale = Snapshot(
            number=3, title="slow", author="c",
            labels=frozenset({NEEDS_AUTHOR}),
            state_since=NOW - timedelta(days=12),
        )  # fmt: skip
        fresh_author = Snapshot(
            number=4, title="fresh", author="d",
            labels=frozenset({NEEDS_AUTHOR}),
            state_since=NOW - timedelta(days=1),
        )  # fmt: skip
        body = pr_digest.render([new, old, close_to_stale, fresh_author], NOW)
        self.assertLess(body.index("#1"), body.index("#2"))
        self.assertIn("old \\| pipe", body)
        self.assertIn("9d (overdue)", body)
        self.assertIn("#3", body)
        self.assertNotIn("#4", body)


if __name__ == "__main__":
    unittest.main()
