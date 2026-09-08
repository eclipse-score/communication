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

"""Tests for dismiss_and_invalidate.py."""

from __future__ import annotations

from datetime import datetime, timezone
from unittest.mock import MagicMock, patch

import pytest
import sys

from dismiss_and_invalidate import (
    _get_files_in_latest_push,
    handle_synchronize,
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


def _make_review(user_login, state, review_id=1):
    r = MagicMock()
    r.user.login = user_login
    r.state = state
    r.id = review_id
    return r


def _make_run(run_id, head_sha):
    r = MagicMock()
    r.id = run_id
    r.head_sha = head_sha
    return r


SAMPLE_CHECKLISTS = [
    {
        "id": "api-review",
        "name": "API Review",
        "include": ["src/api/*.py"],
        "checklist": "- [ ] Reviewed",
    },
]


# ---------------------------------------------------------------------------
# _get_files_in_latest_push
# ---------------------------------------------------------------------------


class TestGetFilesInLatestPush:
    def test_falls_back_when_no_run_context(self, monkeypatch):
        monkeypatch.delenv("CURRENT_RUN_ID", raising=False)
        monkeypatch.delenv("HEAD_BRANCH", raising=False)

        pr = MagicMock()
        pr.get_files.return_value = [_make_file("fallback.txt")]
        repo = MagicMock()

        result = _get_files_in_latest_push(pr, repo)

        assert result == ["fallback.txt"]
        repo.get_workflow.assert_not_called()

    def test_uses_previous_run_head_sha_for_comparison(self, monkeypatch):
        monkeypatch.setenv("CURRENT_RUN_ID", "20")
        monkeypatch.setenv("HEAD_BRANCH", "feature")

        pr = MagicMock()
        pr.head.sha = "newsha"

        repo = MagicMock()
        workflow = MagicMock()
        # Newest-first, as returned by the GitHub API.
        workflow.get_runs.return_value = [
            _make_run(20, "newsha"),
            _make_run(10, "oldsha"),
        ]
        repo.get_workflow.return_value = workflow
        comparison = MagicMock()
        comparison.files = [_make_file("src/api/handler.py")]
        repo.compare.return_value = comparison

        result = _get_files_in_latest_push(pr, repo)

        repo.get_workflow.assert_called_once_with("review_checklists_trigger.yml")
        workflow.get_runs.assert_called_once_with(branch="feature", event="pull_request_target")
        repo.compare.assert_called_once_with("oldsha", "newsha")
        assert result == ["src/api/handler.py"]

    def test_falls_back_when_previous_run_not_found(self, monkeypatch):
        monkeypatch.setenv("CURRENT_RUN_ID", "999")
        monkeypatch.setenv("HEAD_BRANCH", "feature")

        pr = MagicMock()
        pr.get_files.return_value = [_make_file("fallback.txt")]

        repo = MagicMock()
        workflow = MagicMock()
        workflow.get_runs.return_value = [_make_run(20, "newsha")]
        repo.get_workflow.return_value = workflow

        result = _get_files_in_latest_push(pr, repo)

        assert result == ["fallback.txt"]
        repo.compare.assert_not_called()

    def test_falls_back_when_current_run_is_first(self, monkeypatch):
        # Current run has no predecessor (e.g. only one run exists).
        monkeypatch.setenv("CURRENT_RUN_ID", "20")
        monkeypatch.setenv("HEAD_BRANCH", "feature")

        pr = MagicMock()
        pr.get_files.return_value = [_make_file("fallback.txt")]

        repo = MagicMock()
        workflow = MagicMock()
        workflow.get_runs.return_value = [_make_run(20, "newsha")]
        repo.get_workflow.return_value = workflow

        result = _get_files_in_latest_push(pr, repo)

        assert result == ["fallback.txt"]
        repo.compare.assert_not_called()


# ---------------------------------------------------------------------------
# handle_synchronize
# ---------------------------------------------------------------------------


class TestHandleSynchronize:
    @patch("dismiss_and_invalidate.set_commit_status")
    @patch("dismiss_and_invalidate.load_checklists", return_value=SAMPLE_CHECKLISTS)
    def test_no_affected_checklists(
        self,
        mock_load,
        mock_status,
        monkeypatch,
    ):
        monkeypatch.delenv("CURRENT_RUN_ID", raising=False)
        monkeypatch.delenv("HEAD_BRANCH", raising=False)

        pr = MagicMock()
        pr.get_files.return_value = [_make_file("unrelated.txt")]
        repo = MagicMock()

        handle_synchronize(pr, repo, ".github/review_checklists.yml")

        mock_status.assert_not_called()

    @patch("dismiss_and_invalidate.set_commit_status")
    @patch("dismiss_and_invalidate.load_checklists", return_value=SAMPLE_CHECKLISTS)
    def test_deletes_ok_without_dismissing(
        self,
        mock_load,
        mock_status,
        monkeypatch,
    ):
        monkeypatch.delenv("CURRENT_RUN_ID", raising=False)
        monkeypatch.delenv("HEAD_BRANCH", raising=False)

        pr = MagicMock()
        pr.head.sha = "newsha"
        pr.get_files.return_value = [_make_file("src/api/handler.py")]
        repo = MagicMock()

        cl_comment = MagicMock()
        cl_comment.id = 100
        cl_comment.body = "<!-- review-checklist:api-review -->"

        ok_reply = _make_comment(
            101,
            "OK",
            "alice",
            datetime(2026, 1, 1, 0, 1, tzinfo=timezone.utc),
        )
        ok_reply.in_reply_to_id = 100
        pr.get_review_comments.return_value = [ok_reply]

        review = _make_review("alice", "APPROVED", 42)
        pr.get_reviews.return_value = [review]

        with patch(
            "dismiss_and_invalidate.find_existing_checklist_comments",
            return_value={"api-review": cl_comment},
        ):
            handle_synchronize(pr, repo, ".github/review_checklists.yml")

        ok_reply.delete.assert_called_once()
        review.dismiss.assert_not_called()
        mock_status.assert_called_once_with(
            repo,
            "newsha",
            "pending",
            "Checklist acknowledgements invalidated due to new changes",
        )


if __name__ == "__main__":
    sys.exit(pytest.main(sys.argv[1:]))
