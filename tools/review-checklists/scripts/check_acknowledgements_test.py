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

"""Tests for check_acknowledgements.py."""

from __future__ import annotations

import json
import os
from datetime import datetime, timezone
from unittest.mock import MagicMock, patch

import pytest
import sys

from check_acknowledgements import (
    _collect_ok_acknowledgements,
    _validate_checklist_evidence,
    main,
)


def _make_comment(comment_id, body, user_login="reviewer", created_at=None):
    c = MagicMock()
    c.id = comment_id
    c.body = body
    c.user.login = user_login
    c.created_at = created_at or datetime(2026, 1, 1, tzinfo=timezone.utc)
    return c


def _make_file(filename):
    f = MagicMock()
    f.filename = filename
    return f


def _make_review(user_login, state):
    r = MagicMock()
    r.user.login = user_login
    r.state = state
    return r


SAMPLE_CHECKLISTS = [
    {
        "id": "api-review",
        "name": "API Review",
        "include": ["src/api/*.py"],
        "checklist": "- [ ] Reviewed",
    },
]

TWO_SAMPLE_CHECKLISTS = [
    {
        "id": "api-review",
        "name": "API Review",
        "include": ["src/api/*.py"],
        "checklist": "- [ ] Reviewed",
    },
    {
        "id": "doc-review",
        "name": "Doc Review",
        "include": ["docs/*.md"],
        "checklist": "- [ ] Reviewed",
    },
]


# ---------------------------------------------------------------------------
# _collect_ok_acknowledgements
# ---------------------------------------------------------------------------


class TestCollectOkAcknowledgements:
    def test_ok_reply_recognized(self):
        """A reply with OK is recognized."""
        cl_comment = MagicMock(id=100)
        ok_reply = _make_comment(
            101,
            "OK",
            "alice",
            datetime(2026, 1, 1, 0, 1, tzinfo=timezone.utc),
        )
        ok_reply.in_reply_to_id = 100
        pr = MagicMock()
        pr.get_review_comments.return_value = [ok_reply]

        existing = {"api-review": cl_comment}
        acks = _collect_ok_acknowledgements(pr, existing, ["api-review"])
        assert "alice" in acks["api-review"]

    def test_bare_ok_reply_recognized(self):
        """A bare 'OK' reply is recognized."""
        cl_comment = MagicMock(id=100)
        ok_reply = _make_comment(
            101,
            "OK",
            "bob",
            datetime(2026, 1, 1, 0, 1, tzinfo=timezone.utc),
        )
        ok_reply.in_reply_to_id = 100
        pr = MagicMock()
        pr.get_review_comments.return_value = [ok_reply]

        existing = {"api-review": cl_comment}
        acks = _collect_ok_acknowledgements(pr, existing, ["api-review"])
        assert "bob" in acks["api-review"]

    def test_reply_to_different_comment_ignored(self):
        """A reply to a different comment is not counted."""
        cl_comment = MagicMock(id=100)
        ok_reply = _make_comment(
            101,
            "OK",
            "bob",
            datetime(2026, 1, 1, 0, 1, tzinfo=timezone.utc),
        )
        ok_reply.in_reply_to_id = 999  # different comment
        pr = MagicMock()
        pr.get_review_comments.return_value = [ok_reply]

        existing = {"api-review": cl_comment}
        acks = _collect_ok_acknowledgements(pr, existing, ["api-review"])
        assert acks["api-review"] == set()

    def test_multiple_reviewers(self):
        """Two reviewers both replying OK are collected."""
        cl_comment = MagicMock(id=100)
        ok1 = _make_comment(
            101,
            "OK",
            "alice",
            datetime(2026, 1, 1, 0, 1, tzinfo=timezone.utc),
        )
        ok1.in_reply_to_id = 100
        ok2 = _make_comment(
            102,
            "OK",
            "bob",
            datetime(2026, 1, 1, 0, 2, tzinfo=timezone.utc),
        )
        ok2.in_reply_to_id = 100
        pr = MagicMock()
        pr.get_review_comments.return_value = [ok1, ok2]

        existing = {"api-review": cl_comment}
        acks = _collect_ok_acknowledgements(pr, existing, ["api-review"])
        assert acks["api-review"] == {"alice", "bob"}

    def test_unrelated_reply_ignored(self):
        """A reply that is not OK is not counted."""
        cl_comment = MagicMock(id=100)
        normal = _make_comment(
            101,
            "Looks good to me!",
            "alice",
            datetime(2026, 1, 1, 0, 1, tzinfo=timezone.utc),
        )
        normal.in_reply_to_id = 100
        pr = MagicMock()
        pr.get_review_comments.return_value = [normal]

        existing = {"api-review": cl_comment}
        acks = _collect_ok_acknowledgements(pr, existing, ["api-review"])
        assert acks["api-review"] == set()


# ---------------------------------------------------------------------------
# _validate_checklist_evidence
# ---------------------------------------------------------------------------


class TestValidateChecklistEvidence:
    def test_pending_when_not_all_relevant_checklists_posted(self):
        """Regression test for 631b5ff6.

        With two relevant checklists but only one posted comment, the old
        code derived 'relevant_ids' as only the posted subset and treated
        a *non-empty* relevant_ids as "all posted", so it went on to
        evaluate acknowledgements for just that posted subset and could
        reach "success" while doc-review was never even posted. It must
        stay pending until every relevant checklist has a posted comment.
        """
        pr = MagicMock()
        pr.get_files.return_value = [
            _make_file("src/api/foo.py"),
            _make_file("docs/readme.md"),
        ]

        api_review_comment = MagicMock(id=100)
        api_review_comment.body = "<!-- review-checklist:api-review -->"

        with patch(
            "check_acknowledgements.find_existing_checklist_comments",
            # Only api-review has been posted; doc-review has not.
            return_value={"api-review": api_review_comment},
        ):
            state, description = _validate_checklist_evidence(pr, TWO_SAMPLE_CHECKLISTS)

        assert state == "pending"
        assert description == "Checklist comments not yet posted"

    def test_success_when_all_relevant_checklists_posted(self):
        """Sanity check: once every relevant checklist has been posted and
        acknowledged, evidence validation is not blocked."""
        pr = MagicMock()
        pr.get_files.return_value = [_make_file("src/api/foo.py")]

        api_review_comment = MagicMock(id=100)
        api_review_comment.body = "<!-- review-checklist:api-review -->"

        ok_reply = _make_comment(
            101,
            "OK",
            "alice",
            datetime(2026, 1, 1, 0, 1, tzinfo=timezone.utc),
        )
        ok_reply.in_reply_to_id = 100
        pr.get_review_comments.return_value = [ok_reply]

        with (
            patch(
                "check_acknowledgements.find_existing_checklist_comments",
                return_value={"api-review": api_review_comment},
            ),
            patch(
                "check_acknowledgements.get_approving_reviewers",
                return_value=["alice"],
            ),
        ):
            state, description = _validate_checklist_evidence(pr, SAMPLE_CHECKLISTS)

        assert state == "success"
        assert description == "All checklists acknowledged by all approving reviewers"


# ---------------------------------------------------------------------------
# main()
# ---------------------------------------------------------------------------


class TestCheckAcknowledgementsMain:
    @patch("check_acknowledgements.is_pr_in_merge_queue", return_value=False)
    @patch("check_acknowledgements.set_commit_status")
    @patch(
        "check_acknowledgements.load_checklists",
        return_value=SAMPLE_CHECKLISTS,
    )
    @patch("check_acknowledgements.get_repo_and_pr")
    @patch("check_acknowledgements.get_github_client")
    def test_no_relevant_checklists_sets_success(self, mock_gh, mock_repo_pr, mock_load, mock_status, mock_in_queue):
        repo = MagicMock()
        pr = MagicMock()
        pr.head.sha = "abc"
        pr.get_files.return_value = [_make_file("unrelated.txt")]
        mock_repo_pr.return_value = (repo, pr)
        # Ensure the argument parser doesn't see pytest's CLI arguments.
        with patch.object(sys, "argv", ["check_acknowledgements"]):
            main()
        mock_status.assert_called_once_with(repo, "abc", "success", "No checklists applicable")

    @patch("check_acknowledgements.is_pr_in_merge_queue", return_value=False)
    @patch("check_acknowledgements.set_commit_status")
    @patch(
        "check_acknowledgements.find_existing_checklist_comments",
        return_value={},
    )
    @patch(
        "check_acknowledgements.load_checklists",
        return_value=SAMPLE_CHECKLISTS,
    )
    @patch("check_acknowledgements.get_repo_and_pr")
    @patch("check_acknowledgements.get_github_client")
    def test_no_existing_comments_sets_pending(
        self, mock_gh, mock_repo_pr, mock_load, mock_existing, mock_status, mock_in_queue
    ):
        repo = MagicMock()
        pr = MagicMock()
        pr.head.sha = "abc"
        pr.get_files.return_value = [_make_file("src/api/foo.py")]
        mock_repo_pr.return_value = (repo, pr)

        # Ensure the argument parser doesn't see pytest's CLI arguments.
        with patch.object(sys, "argv", ["check_acknowledgements"]):
            main()

        mock_status.assert_called_once_with(repo, "abc", "pending", "Checklist comments not yet posted")

    @patch("check_acknowledgements.is_pr_in_merge_queue", return_value=False)
    @patch("check_acknowledgements.set_commit_status")
    @patch(
        "check_acknowledgements.load_checklists",
        return_value=TWO_SAMPLE_CHECKLISTS,
    )
    @patch("check_acknowledgements.get_repo_and_pr")
    @patch("check_acknowledgements.get_github_client")
    def test_some_but_not_all_relevant_checklists_posted_sets_pending(
        self, mock_gh, mock_repo_pr, mock_load, mock_status, mock_in_queue
    ):
        """Regression test for 631b5ff6 (main()'s live-PR flow).

        Both api-review and doc-review are relevant, but only api-review
        has a posted finding comment. The old code derived 'relevant_ids'
        as just the posted subset and treated it as non-empty, silently
        dropping doc-review from consideration entirely instead of
        staying pending until it too is posted.
        """
        repo = MagicMock()
        pr = MagicMock()
        pr.head.sha = "abc"
        pr.get_files.return_value = [
            _make_file("src/api/foo.py"),
            _make_file("docs/readme.md"),
        ]
        mock_repo_pr.return_value = (repo, pr)

        api_review_comment = MagicMock(id=100)
        api_review_comment.body = "<!-- review-checklist:api-review -->"

        with (
            patch(
                "check_acknowledgements.find_existing_checklist_comments",
                # Only api-review has been posted; doc-review has not.
                return_value={"api-review": api_review_comment},
            ),
            # Ensure the argument parser doesn't see pytest's CLI arguments.
            patch.object(sys, "argv", ["check_acknowledgements"]),
        ):
            main()

        mock_status.assert_called_once_with(repo, "abc", "pending", "Checklist comments not yet posted")

    @patch("check_acknowledgements.is_pr_in_merge_queue", return_value=False)
    @patch("check_acknowledgements.set_commit_status")
    @patch("check_acknowledgements.get_approving_reviewers", return_value=[])
    @patch(
        "check_acknowledgements.load_checklists",
        return_value=SAMPLE_CHECKLISTS,
    )
    @patch("check_acknowledgements.get_repo_and_pr")
    @patch("check_acknowledgements.get_github_client")
    def test_no_approvers_sets_pending(
        self, mock_gh, mock_repo_pr, mock_load, mock_approvers, mock_status, mock_in_queue
    ):
        repo = MagicMock()
        pr = MagicMock()
        pr.head.sha = "abc"
        pr.get_files.return_value = [_make_file("src/api/foo.py")]

        cl_review = MagicMock()
        cl_review.id = 100
        cl_review.body = "<!-- review-checklist:api-review -->"
        pr.get_review_comments.return_value = []
        mock_repo_pr.return_value = (repo, pr)

        with (
            patch(
                "check_acknowledgements.find_existing_checklist_comments",
                return_value={"api-review": cl_review},
            ),
            # Ensure the argument parser doesn't see pytest's CLI arguments.
            patch.object(sys, "argv", ["check_acknowledgements"]),
        ):
            main()

        mock_status.assert_called_with(repo, "abc", "pending", "Awaiting at least one approving review")

    @patch("check_acknowledgements.is_pr_in_merge_queue", return_value=False)
    @patch("check_acknowledgements.set_commit_status")
    @patch(
        "check_acknowledgements.get_approving_reviewers",
        return_value=["alice"],
    )
    @patch(
        "check_acknowledgements.load_checklists",
        return_value=SAMPLE_CHECKLISTS,
    )
    @patch("check_acknowledgements.get_repo_and_pr")
    @patch("check_acknowledgements.get_github_client")
    def test_all_acked_sets_success(
        self,
        mock_gh,
        mock_repo_pr,
        mock_load,
        mock_approvers,
        mock_status,
        mock_in_queue,
        tmp_path,
    ):
        repo = MagicMock()
        pr = MagicMock()
        pr.head.sha = "abc"
        pr.get_files.return_value = [_make_file("src/api/foo.py")]

        cl_review = MagicMock()
        cl_review.id = 100
        cl_review.body = "<!-- review-checklist:api-review -->"

        ok_reply = _make_comment(
            101,
            "OK",
            "alice",
            datetime(2026, 1, 1, 0, 1, tzinfo=timezone.utc),
        )
        ok_reply.in_reply_to_id = 100
        pr.get_review_comments.return_value = [ok_reply]
        mock_repo_pr.return_value = (repo, pr)

        ack_path = str(tmp_path / "acks.json")

        with (
            patch(
                "check_acknowledgements.find_existing_checklist_comments",
                return_value={"api-review": cl_review},
            ),
            patch.dict(os.environ, {"ACK_OUTPUT_PATH": ack_path}),
            # Ensure the argument parser doesn't see pytest's CLI arguments.
            patch.object(sys, "argv", ["check_acknowledgements"]),
        ):
            main()

        # The last call should be success.
        mock_status.assert_called_with(
            repo,
            "abc",
            "success",
            "All checklists acknowledged by all approving reviewers",
        )
        # Check that ack data was written.
        with open(ack_path) as f:
            data = json.load(f)
        assert data["api-review"] == ["alice"]

    @patch("check_acknowledgements.is_pr_in_merge_queue", return_value=False)
    @patch("check_acknowledgements.set_commit_status")
    @patch(
        "check_acknowledgements.get_approving_reviewers",
        return_value=["alice", "bob"],
    )
    @patch(
        "check_acknowledgements.load_checklists",
        return_value=SAMPLE_CHECKLISTS,
    )
    @patch("check_acknowledgements.get_repo_and_pr")
    @patch("check_acknowledgements.get_github_client")
    def test_missing_ack_sets_pending(
        self,
        mock_gh,
        mock_repo_pr,
        mock_load,
        mock_approvers,
        mock_status,
        mock_in_queue,
        tmp_path,
    ):
        repo = MagicMock()
        pr = MagicMock()
        pr.head.sha = "abc"
        pr.get_files.return_value = [_make_file("src/api/foo.py")]

        cl_review = MagicMock()
        cl_review.id = 100
        cl_review.body = "<!-- review-checklist:api-review -->"

        # Only alice acked, bob did not.
        ok_reply = _make_comment(
            101,
            "OK",
            "alice",
            datetime(2026, 1, 1, 0, 1, tzinfo=timezone.utc),
        )
        ok_reply.in_reply_to_id = 100
        pr.get_review_comments.return_value = [ok_reply]
        mock_repo_pr.return_value = (repo, pr)

        ack_path = str(tmp_path / "acks.json")

        with (
            patch(
                "check_acknowledgements.find_existing_checklist_comments",
                return_value={"api-review": cl_review},
            ),
            patch.dict(os.environ, {"ACK_OUTPUT_PATH": ack_path}),
            # Ensure the argument parser doesn't see pytest's CLI arguments.
            patch.object(sys, "argv", ["check_acknowledgements"]),
        ):
            main()

        mock_status.assert_called_with(repo, "abc", "pending", "api-review: awaiting bob")

    @patch("check_acknowledgements.ensure_merge_queue_notice_description")
    @patch("check_acknowledgements.ensure_merge_queue_notice_comment")
    @patch("check_acknowledgements.is_pr_in_merge_queue", return_value=True)
    @patch("check_acknowledgements.set_commit_status")
    @patch(
        "check_acknowledgements.load_checklists",
        return_value=SAMPLE_CHECKLISTS,
    )
    @patch("check_acknowledgements.get_repo_and_pr")
    @patch("check_acknowledgements.get_github_client")
    def test_merge_queue_notice_refreshed_when_pr_enqueued(
        self,
        mock_gh,
        mock_repo_pr,
        mock_load,
        mock_status,
        mock_in_queue,
        mock_notice_comment,
        mock_notice_description,
    ):
        repo = MagicMock()
        pr = MagicMock()
        pr.head.sha = "abc"
        pr.get_files.return_value = [_make_file("unrelated.txt")]
        mock_repo_pr.return_value = (repo, pr)

        with patch.object(sys, "argv", ["check_acknowledgements"]):
            main()

        mock_notice_comment.assert_called_once_with(pr)
        mock_notice_description.assert_called_once_with(pr)

    @patch("check_acknowledgements.set_commit_status")
    @patch("check_acknowledgements.get_repo_and_pr")
    @patch("check_acknowledgements.get_github_client")
    def test_merge_group_without_pr_number_sets_failure(self, mock_gh, mock_repo_pr, mock_status):
        repo = MagicMock()
        gh = MagicMock()
        gh.get_repo.return_value = repo
        mock_gh.return_value = gh

        with (
            patch.dict(
                os.environ,
                {
                    "CHECKLISTS_EVENT_NAME": "merge_group",
                    "HEAD_SHA": "merge123",
                    "GITHUB_REPOSITORY": "acme/widgets",
                },
            ),
            # Ensure the argument parser doesn't see pytest's CLI arguments.
            patch.object(sys, "argv", ["check_acknowledgements"]),
        ):
            with pytest.raises(SystemExit):
                main()

        mock_repo_pr.assert_not_called()
        mock_status.assert_called_once_with(
            repo,
            "merge123",
            "failure",
            "Could not resolve pull request to validate checklist evidence",
        )

    @patch("check_acknowledgements.set_commit_status")
    @patch(
        "check_acknowledgements.get_approving_reviewers",
        return_value=["alice"],
    )
    @patch(
        "check_acknowledgements.load_checklists",
        return_value=SAMPLE_CHECKLISTS,
    )
    @patch("check_acknowledgements.get_repo_and_pr")
    @patch("check_acknowledgements.get_github_client")
    def test_merge_group_validates_evidence_and_sets_success(
        self, mock_gh, mock_repo_pr, mock_load, mock_approvers, mock_status
    ):
        repo = MagicMock()
        pr = MagicMock()
        pr.get_files.return_value = [_make_file("src/api/foo.py")]

        cl_review = MagicMock()
        cl_review.id = 100
        cl_review.body = "<!-- review-checklist:api-review -->"

        ok_reply = _make_comment(
            101,
            "OK",
            "alice",
            datetime(2026, 1, 1, 0, 1, tzinfo=timezone.utc),
        )
        ok_reply.in_reply_to_id = 100
        pr.get_review_comments.return_value = [ok_reply]

        gh = MagicMock()
        gh.get_repo.return_value = repo
        mock_gh.return_value = gh
        repo.get_pull.return_value = pr

        with (
            patch(
                "check_acknowledgements.find_existing_checklist_comments",
                return_value={"api-review": cl_review},
            ),
            patch.dict(
                os.environ,
                {
                    "CHECKLISTS_EVENT_NAME": "merge_group",
                    "HEAD_SHA": "merge123",
                    "GITHUB_REPOSITORY": "acme/widgets",
                    "PR_NUMBER": "75",
                },
            ),
            patch.object(sys, "argv", ["check_acknowledgements"]),
        ):
            main()

        repo.get_pull.assert_called_once_with(75)
        mock_repo_pr.assert_not_called()
        mock_status.assert_called_once_with(
            repo,
            "merge123",
            "success",
            "Merge queue: All checklists acknowledged by all approving reviewers",
        )

    @patch("check_acknowledgements.set_commit_status")
    @patch(
        "check_acknowledgements.get_approving_reviewers",
        return_value=["alice", "bob"],
    )
    @patch(
        "check_acknowledgements.load_checklists",
        return_value=SAMPLE_CHECKLISTS,
    )
    @patch("check_acknowledgements.get_repo_and_pr")
    @patch("check_acknowledgements.get_github_client")
    def test_merge_group_validates_evidence_and_sets_pending_on_missing_ack(
        self, mock_gh, mock_repo_pr, mock_load, mock_approvers, mock_status
    ):
        repo = MagicMock()
        pr = MagicMock()
        pr.get_files.return_value = [_make_file("src/api/foo.py")]

        cl_review = MagicMock()
        cl_review.id = 100
        cl_review.body = "<!-- review-checklist:api-review -->"

        # Only alice acked, bob (also an approver) did not.
        ok_reply = _make_comment(
            101,
            "OK",
            "alice",
            datetime(2026, 1, 1, 0, 1, tzinfo=timezone.utc),
        )
        ok_reply.in_reply_to_id = 100
        pr.get_review_comments.return_value = [ok_reply]

        gh = MagicMock()
        gh.get_repo.return_value = repo
        mock_gh.return_value = gh
        repo.get_pull.return_value = pr

        with (
            patch(
                "check_acknowledgements.find_existing_checklist_comments",
                return_value={"api-review": cl_review},
            ),
            patch.dict(
                os.environ,
                {
                    "CHECKLISTS_EVENT_NAME": "merge_group",
                    "HEAD_SHA": "merge123",
                    "GITHUB_REPOSITORY": "acme/widgets",
                    "PR_NUMBER": "75",
                },
            ),
            patch.object(sys, "argv", ["check_acknowledgements"]),
        ):
            with pytest.raises(SystemExit):
                main()

        mock_status.assert_called_once_with(
            repo,
            "merge123",
            "pending",
            "Merge queue: api-review: awaiting bob",
        )


if __name__ == "__main__":
    sys.exit(pytest.main(sys.argv[1:]))
