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

"""Tests for post_checklists.py."""

from __future__ import annotations

from unittest.mock import MagicMock, patch

import pytest
import sys

from post_checklists import main


def _make_file(filename):
    f = MagicMock()
    f.filename = filename
    return f


SAMPLE_CHECKLISTS = [
    {
        "id": "api-review",
        "name": "API Review",
        "include": ["src/api/*.py"],
        "checklist": "- [ ] Reviewed",
    },
]


class TestPostChecklistsMain:
    """Integration-level tests for the main() entry point."""

    @patch("post_checklists.set_commit_status")
    @patch("post_checklists.load_checklists", return_value=SAMPLE_CHECKLISTS)
    @patch("post_checklists.get_repo_and_pr")
    @patch("post_checklists.get_github_client")
    def test_no_relevant_checklists_sets_success(self, mock_gh, mock_repo_pr, mock_load, mock_status):
        repo = MagicMock()
        pr = MagicMock()
        pr.head.sha = "abc123"
        pr.get_files.return_value = [_make_file("unrelated/file.txt")]
        mock_repo_pr.return_value = (repo, pr)

        main()

        mock_status.assert_called_once_with(repo, "abc123", "success", "No checklists applicable")

    @patch("post_checklists.set_commit_status")
    @patch("post_checklists.find_existing_checklist_comments", return_value={})
    @patch("post_checklists.load_checklists", return_value=SAMPLE_CHECKLISTS)
    @patch("post_checklists.get_repo_and_pr")
    @patch("post_checklists.get_github_client")
    def test_creates_new_review_with_inline_comment(self, mock_gh, mock_repo_pr, mock_load, mock_existing, mock_status):
        repo = MagicMock()
        pr = MagicMock()
        pr.head.sha = "abc123"
        pr.get_files.return_value = [_make_file("src/api/handler.py")]
        mock_repo_pr.return_value = (repo, pr)

        main()

        pr.create_review_comment.assert_called_once()
        call_kwargs = pr.create_review_comment.call_args[1]
        assert call_kwargs["commit"] == "abc123"
        assert call_kwargs["path"] == "src/api/handler.py"
        assert call_kwargs["subject_type"] == "file"
        assert "api-review" in call_kwargs["body"]
        mock_status.assert_called_with(
            repo,
            "abc123",
            "pending",
            "1 checklist(s) require reviewer acknowledgement",
        )

    @patch("post_checklists.set_commit_status")
    @patch("post_checklists.load_checklists", return_value=SAMPLE_CHECKLISTS)
    @patch("post_checklists.get_repo_and_pr")
    @patch("post_checklists.get_github_client")
    def test_updates_existing_review_when_body_changed(self, mock_gh, mock_repo_pr, mock_load, mock_status):
        repo = MagicMock()
        pr = MagicMock()
        pr.head.sha = "abc123"
        pr.get_files.return_value = [_make_file("src/api/handler.py")]

        existing_review = MagicMock()
        existing_review.body = "old body"

        with patch(
            "post_checklists.find_existing_checklist_comments",
            return_value={"api-review": existing_review},
        ):
            mock_repo_pr.return_value = (repo, pr)
            main()

        existing_review.edit.assert_called_once()

    @patch("post_checklists.set_commit_status")
    @patch("post_checklists.load_checklists", return_value=SAMPLE_CHECKLISTS)
    @patch("post_checklists.get_repo_and_pr")
    @patch("post_checklists.get_github_client")
    def test_skips_update_when_body_unchanged(self, mock_gh, mock_repo_pr, mock_load, mock_status):
        repo = MagicMock()
        pr = MagicMock()
        pr.head.sha = "abc123"
        pr.get_files.return_value = [_make_file("src/api/handler.py")]

        # Import to build expected body
        from helpers import make_checklist_comment_body

        expected_body = make_checklist_comment_body(SAMPLE_CHECKLISTS[0])

        existing_review = MagicMock()
        existing_review.body = expected_body

        with patch(
            "post_checklists.find_existing_checklist_comments",
            return_value={"api-review": existing_review},
        ):
            mock_repo_pr.return_value = (repo, pr)
            main()

        existing_review.edit.assert_not_called()


if __name__ == "__main__":
    sys.exit(pytest.main(sys.argv[1:]))
