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

"""Tests for helpers.py."""

from __future__ import annotations

import sys
import types
from unittest.mock import MagicMock, patch

import pytest
import yaml

from helpers import (
    CHECKLIST_MARKER,
    MERGE_QUEUE_COMMENT_MARKER,
    MERGE_QUEUE_NOTICE,
    MERGE_QUEUE_NOTICE_END,
    MERGE_QUEUE_NOTICE_START,
    OK_KEYWORD,
    _find_checklists_config,
    collect_acknowledgement_details,
    ensure_merge_queue_notice_comment,
    ensure_merge_queue_notice_description,
    find_existing_checklist_comments,
    find_ok_replies_for_checklists,
    get_approving_reviewers,
    get_changed_files,
    get_github_client,
    get_repo_and_pr,
    is_pr_in_merge_queue,
    load_checklists,
    make_checklist_comment_body,
    match_checklists,
    set_commit_status,
)


# ---------------------------------------------------------------------------
# Fixtures
# ---------------------------------------------------------------------------

SAMPLE_CHECKLISTS = [
    {
        "id": "api-review",
        "name": "API Review",
        "include": ["src/api/*.py", "src/api/*.h"],
        "checklist": "- [ ] APIs documented\n- [ ] Tests added",
    },
    {
        "id": "docs-review",
        "name": "Documentation Review",
        "include": ["docs/**"],
        "checklist": "- [ ] Spelling checked",
    },
    {
        "id": "build-review",
        "name": "Build Review",
        "include": ["**/BUILD", "**/*.bzl"],
        "checklist": "- [ ] Targets correct",
    },
    {
        "id": "com-review",
        "name": "COM Review",
        "include": ["score/mw/com/**"],
        "exclude": ["score/mw/com/design/**", "score/mw/com/impl/**"],
        "checklist": "- [ ] API reviewed",
    },
]


@pytest.fixture()
def sample_config(tmp_path):
    """Write a sample review_checklists.yml and return its path."""
    cfg = tmp_path / "review_checklists.yml"
    cfg.write_text(yaml.dump({"checklists": SAMPLE_CHECKLISTS}))
    return str(cfg)


def _make_comment(comment_id, body, user_login="bot", created_at=None):
    """Build a lightweight mock issue-comment."""
    from datetime import datetime, timezone

    c = MagicMock()
    c.id = comment_id
    c.body = body
    c.user.login = user_login
    c.created_at = created_at or datetime(2026, 1, 1, tzinfo=timezone.utc)
    return c


def _make_review(user_login, state, review_id=1, body=None):
    r = MagicMock()
    r.user.login = user_login
    r.state = state
    r.id = review_id
    r.body = body or ""
    return r


# ---------------------------------------------------------------------------
# get_github_client / get_repo_and_pr
# ---------------------------------------------------------------------------


class TestGetGithubClient:
    def test_reads_token_from_env(self, monkeypatch):
        monkeypatch.setenv("GITHUB_TOKEN", "ghp_test123")
        with patch("helpers.Github") as mock_cls:
            get_github_client()
            mock_cls.assert_called_once_with("ghp_test123")

    def test_missing_token_raises(self, monkeypatch):
        monkeypatch.delenv("GITHUB_TOKEN", raising=False)
        with pytest.raises(KeyError):
            get_github_client()


class TestGetRepoAndPr:
    def test_returns_repo_and_pr(self, monkeypatch):
        monkeypatch.setenv("GITHUB_REPOSITORY", "org/repo")
        monkeypatch.setenv("PR_NUMBER", "42")
        gh = MagicMock()
        repo, pr = get_repo_and_pr(gh)
        gh.get_repo.assert_called_once_with("org/repo")
        gh.get_repo.return_value.get_pull.assert_called_once_with(42)


# ---------------------------------------------------------------------------
# load_checklists
# ---------------------------------------------------------------------------


class TestLoadChecklists:
    def test_load_from_explicit_path(self, sample_config):
        with patch("helpers._find_checklists_config", return_value=sample_config):
            result = load_checklists()
        assert len(result) == 4
        assert result[0]["id"] == "api-review"

    def test_file_not_found_raises(self, monkeypatch, tmp_path):
        monkeypatch.delenv("RUNFILES_DIR", raising=False)
        monkeypatch.delenv("RUNFILES_MANIFEST_FILE", raising=False)
        with patch(
            "helpers._find_checklists_config",
            side_effect=FileNotFoundError("Cannot locate .github/review_checklists.yml"),
        ):
            with pytest.raises(FileNotFoundError):
                load_checklists()


# ---------------------------------------------------------------------------
# get_changed_files
# ---------------------------------------------------------------------------


class TestGetChangedFiles:
    def test_returns_filenames(self):
        file1 = MagicMock()
        file1.filename = "src/api/foo.py"
        file2 = MagicMock()
        file2.filename = "docs/readme.md"
        pr = MagicMock()
        pr.get_files.return_value = [file1, file2]
        assert get_changed_files(pr) == ["src/api/foo.py", "docs/readme.md"]


# ---------------------------------------------------------------------------
# match_checklists
# ---------------------------------------------------------------------------


class TestMatchChecklists:
    def test_single_match(self):
        files = ["src/api/handler.py"]
        result = match_checklists(SAMPLE_CHECKLISTS, files)
        assert len(result) == 1
        assert result[0]["id"] == "api-review"
        assert result[0]["matched_files"] == ["src/api/handler.py"]

    def test_multiple_matches(self):
        files = ["src/api/handler.py", "docs/guide.md"]
        result = match_checklists(SAMPLE_CHECKLISTS, files)
        ids = {r["id"] for r in result}
        assert ids == {"api-review", "docs-review"}

    def test_no_match(self):
        files = ["unrelated/file.txt"]
        result = match_checklists(SAMPLE_CHECKLISTS, files)
        assert result == []

    def test_glob_double_star(self):
        files = ["docs/nested/deep/file.md"]
        result = match_checklists(SAMPLE_CHECKLISTS, files)
        assert len(result) == 1
        assert result[0]["id"] == "docs-review"

    def test_build_glob(self):
        files = ["some/path/BUILD"]
        result = match_checklists(SAMPLE_CHECKLISTS, files)
        assert len(result) == 1
        assert result[0]["id"] == "build-review"

    def test_build_glob_matches_root_level_too(self):
        # "**/BUILD" (gitignore semantics) also matches a root-level file,
        # unlike a plain fnmatch translation of "**/BUILD".
        files = ["BUILD"]
        result = match_checklists(SAMPLE_CHECKLISTS, files)
        assert len(result) == 1
        assert result[0]["id"] == "build-review"

    def test_unanchored_pattern_matches_root_and_nested(self):
        checklist = [
            {
                "id": "md-anywhere",
                "name": "Markdown anywhere",
                "include": ["*.md"],
                "checklist": "- [ ] Reviewed",
            }
        ]
        files = ["NOTE.md", "docs/NOTE.md", "docs/deep/NOTE.md"]
        result = match_checklists(checklist, files)
        assert len(result) == 1
        assert set(result[0]["matched_files"]) == set(files)

    def test_anchored_pattern_matches_root_only(self):
        checklist = [
            {
                "id": "md-root-only",
                "name": "Markdown at root",
                "include": ["/*.md"],
                "checklist": "- [ ] Reviewed",
            }
        ]
        files = ["NOTE.md", "docs/NOTE.md"]
        result = match_checklists(checklist, files)
        assert len(result) == 1
        assert result[0]["matched_files"] == ["NOTE.md"]

    def test_multiple_files_same_checklist(self):
        files = ["src/api/a.py", "src/api/b.h"]
        result = match_checklists(SAMPLE_CHECKLISTS, files)
        assert len(result) == 1
        assert set(result[0]["matched_files"]) == {
            "src/api/a.py",
            "src/api/b.h",
        }

    def test_does_not_mutate_input(self):
        files = ["src/api/handler.py"]
        original_len = len(SAMPLE_CHECKLISTS[0])
        match_checklists(SAMPLE_CHECKLISTS, files)
        assert len(SAMPLE_CHECKLISTS[0]) == original_len

    def test_exclude_removes_matching_files(self):
        # score/mw/com/design/** is excluded from com-review
        files = ["score/mw/com/design/foo.md"]
        result = match_checklists(SAMPLE_CHECKLISTS, files)
        ids = {r["id"] for r in result}
        assert "com-review" not in ids

    def test_include_minus_exclude_leaves_remainder(self):
        files = [
            "score/mw/com/foo.h",  # included, not excluded
            "score/mw/com/design/bar.md",  # excluded
            "score/mw/com/impl/baz.cpp",  # excluded
        ]
        result = match_checklists(SAMPLE_CHECKLISTS, files)
        com = next((r for r in result if r["id"] == "com-review"), None)
        assert com is not None
        assert com["matched_files"] == ["score/mw/com/foo.h"]

    def test_all_files_excluded_means_no_match(self):
        files = ["score/mw/com/design/only.md", "score/mw/com/impl/only.cpp"]
        result = match_checklists(SAMPLE_CHECKLISTS, files)
        ids = {r["id"] for r in result}
        assert "com-review" not in ids


# ---------------------------------------------------------------------------
# make_checklist_comment_body
# ---------------------------------------------------------------------------


class TestMakeChecklistCommentBody:
    def test_contains_marker(self):
        checklist = SAMPLE_CHECKLISTS[0]
        body = make_checklist_comment_body(checklist)
        expected_marker = CHECKLIST_MARKER.format(checklist_id="api-review")
        assert expected_marker in body

    def test_contains_name(self):
        checklist = SAMPLE_CHECKLISTS[0]
        body = make_checklist_comment_body(checklist)
        assert checklist["name"] in body

    def test_contains_checklist_content(self):
        checklist = SAMPLE_CHECKLISTS[0]
        body = make_checklist_comment_body(checklist)
        assert "APIs documented" in body

    def test_contains_ok_instruction(self):
        checklist = SAMPLE_CHECKLISTS[0]
        body = make_checklist_comment_body(checklist)
        assert OK_KEYWORD in body

    def test_contains_paths(self):
        checklist = SAMPLE_CHECKLISTS[0]
        body = make_checklist_comment_body(checklist)
        for p in checklist["include"]:
            assert p in body

    def test_contains_exclude_when_present(self):
        checklist = SAMPLE_CHECKLISTS[3]  # com-review, has exclude
        body = make_checklist_comment_body(checklist)
        for p in checklist["exclude"]:
            assert p in body

    def test_no_exclude_line_when_absent(self):
        checklist = SAMPLE_CHECKLISTS[0]  # api-review, no exclude
        body = make_checklist_comment_body(checklist)
        assert "Excluding" not in body


# ---------------------------------------------------------------------------
# find_existing_checklist_comments
# ---------------------------------------------------------------------------


class TestFindExistingChecklistComments:
    def test_finds_checklist_review_comments(self):
        c1 = _make_comment(1, "<!-- review-checklist:api-review --> body here")
        c1.in_reply_to_id = None
        c2 = _make_comment(2, "just a normal comment")
        c2.in_reply_to_id = None
        c3 = _make_comment(3, "<!-- review-checklist:docs-review --> docs body")
        c3.in_reply_to_id = None
        pr = MagicMock()
        pr.get_review_comments.return_value = [c1, c2, c3]

        result = find_existing_checklist_comments(pr)
        assert set(result.keys()) == {"api-review", "docs-review"}
        assert result["api-review"].id == 1
        assert result["docs-review"].id == 3

    def test_ignores_reply_comments(self):
        """A reply to a checklist comment should not be treated as a checklist."""
        c1 = _make_comment(1, "<!-- review-checklist:api-review --> body")
        c1.in_reply_to_id = None
        c2 = _make_comment(2, "<!-- review-checklist:api-review --> reply copy")
        c2.in_reply_to_id = 1  # this is a reply
        pr = MagicMock()
        pr.get_review_comments.return_value = [c1, c2]

        result = find_existing_checklist_comments(pr)
        assert len(result) == 1
        assert result["api-review"].id == 1

    def test_returns_empty_when_none(self):
        pr = MagicMock()
        c1 = _make_comment(1, "nothing here")
        c1.in_reply_to_id = None
        pr.get_review_comments.return_value = [c1]
        assert find_existing_checklist_comments(pr) == {}


# ---------------------------------------------------------------------------
# find_ok_replies_for_checklists
# ---------------------------------------------------------------------------


class TestFindOkRepliesForChecklists:
    def test_finds_ok_reply(self):
        c1 = _make_comment(10, "checklist body", "bot")
        c1.in_reply_to_id = None
        c2 = _make_comment(11, "OK", "reviewer1")
        c2.in_reply_to_id = 10
        pr = MagicMock()
        pr.get_review_comments.return_value = [c1, c2]

        result = find_ok_replies_for_checklists(pr, {"api-review": c1}, ["api-review"])
        assert [c.id for c in result["api-review"]] == [11]

    def test_finds_case_insensitive_ok(self):
        for ok in ["OK", "ok", "oK", "Ok"]:
            c1 = _make_comment(10, "checklist body", "bot")
            c1.in_reply_to_id = None
            c2 = _make_comment(11, ok, "reviewer1")
            c2.in_reply_to_id = 10
            pr = MagicMock()
            pr.get_review_comments.return_value = [c1, c2]

            result = find_ok_replies_for_checklists(pr, {"api-review": c1}, ["api-review"])
            assert len(result["api-review"]) == 1

    def test_ignores_reply_to_different_comment(self):
        c1 = _make_comment(10, "checklist body", "bot")
        c1.in_reply_to_id = None
        c2 = _make_comment(11, "OK", "reviewer1")
        c2.in_reply_to_id = 99  # different checklist
        pr = MagicMock()
        pr.get_review_comments.return_value = [c1, c2]

        result = find_ok_replies_for_checklists(pr, {"api-review": c1}, ["api-review"])
        assert result["api-review"] == []

    def test_ignores_unrelated_reply(self):
        c1 = _make_comment(10, "checklist body", "bot")
        c1.in_reply_to_id = None
        c2 = _make_comment(11, "looks good but not OK keyword", "reviewer1")
        c2.in_reply_to_id = 10
        pr = MagicMock()
        pr.get_review_comments.return_value = [c1, c2]

        result = find_ok_replies_for_checklists(pr, {"api-review": c1}, ["api-review"])
        assert result["api-review"] == []


# ---------------------------------------------------------------------------
# collect_acknowledgement_details
# ---------------------------------------------------------------------------


class TestCollectAcknowledgementDetails:
    def test_collects_ok_reply_details_for_relevant_checklists(self):
        checklist_comment = _make_comment(10, "checklist body", "bot")
        ok_reply = _make_comment(11, "OK", "reviewer1")
        ok_reply.in_reply_to_id = 10
        other_reply = _make_comment(12, "looks good", "reviewer2")
        other_reply.in_reply_to_id = 10
        unrelated_reply = _make_comment(13, "OK", "reviewer3")
        unrelated_reply.in_reply_to_id = 999
        pr = MagicMock()
        pr.get_review_comments.return_value = [ok_reply, other_reply, unrelated_reply]

        result = collect_acknowledgement_details(
            pr,
            {"api-review": checklist_comment},
            ["api-review", "docs-review"],
        )

        assert result == {
            "api-review": [
                {
                    "reviewer": "reviewer1",
                    "acknowledged_at": ok_reply.created_at.isoformat(),
                }
            ],
            "docs-review": [],
        }


# ---------------------------------------------------------------------------
# get_approving_reviewers
# ---------------------------------------------------------------------------


class TestGetApprovingReviewers:
    def test_single_approver(self):
        pr = MagicMock()
        pr.get_reviews.return_value = [_make_review("alice", "APPROVED")]
        assert get_approving_reviewers(pr) == ["alice"]

    def test_dismissed_not_counted(self):
        pr = MagicMock()
        pr.get_reviews.return_value = [
            _make_review("alice", "APPROVED"),
            _make_review("alice", "DISMISSED"),
        ]
        assert get_approving_reviewers(pr) == []

    def test_changes_requested_overrides(self):
        pr = MagicMock()
        pr.get_reviews.return_value = [
            _make_review("alice", "APPROVED"),
            _make_review("alice", "CHANGES_REQUESTED"),
        ]
        assert get_approving_reviewers(pr) == []

    def test_re_approval_after_changes_requested(self):
        pr = MagicMock()
        pr.get_reviews.return_value = [
            _make_review("alice", "APPROVED"),
            _make_review("alice", "CHANGES_REQUESTED"),
            _make_review("alice", "APPROVED"),
        ]
        assert get_approving_reviewers(pr) == ["alice"]

    def test_multiple_approvers_sorted(self):
        pr = MagicMock()
        pr.get_reviews.return_value = [
            _make_review("charlie", "APPROVED"),
            _make_review("alice", "APPROVED"),
        ]
        assert get_approving_reviewers(pr) == ["alice", "charlie"]

    def test_no_reviews(self):
        pr = MagicMock()
        pr.get_reviews.return_value = []
        assert get_approving_reviewers(pr) == []


# ---------------------------------------------------------------------------
# set_commit_status
# ---------------------------------------------------------------------------


class TestSetCommitStatus:
    def test_creates_status(self):
        repo = MagicMock()
        set_commit_status(repo, "abc123", "success", "All good")
        commit = repo.get_commit.return_value
        commit.create_status.assert_called_once_with(
            state="success",
            description="All good",
            context="review-checklists",
        )

    def test_truncates_long_description(self):
        repo = MagicMock()
        long_desc = "x" * 200
        set_commit_status(repo, "abc123", "pending", long_desc)
        commit = repo.get_commit.return_value
        call_kwargs = commit.create_status.call_args[1]
        assert len(call_kwargs["description"]) == 140

    def test_custom_context(self):
        repo = MagicMock()
        set_commit_status(repo, "abc123", "success", "ok", context="my-context")
        commit = repo.get_commit.return_value
        call_kwargs = commit.create_status.call_args[1]
        assert call_kwargs["context"] == "my-context"


# ---------------------------------------------------------------------------
# _find_checklists_config
# ---------------------------------------------------------------------------


class TestFindChecklistsConfig:
    def test_find_via_runfiles(self, tmp_path, monkeypatch):
        cfg = tmp_path / "review_checklists.yml"
        cfg.write_text(yaml.dump({"checklists": SAMPLE_CHECKLISTS}))

        class DummyRunfiles:
            def __init__(self, path):
                self._path = path

            def Rlocation(self, _):
                return self._path

            @staticmethod
            def Create():
                return DummyRunfiles(str(cfg))

        runfiles_mod = types.ModuleType("runfiles")
        runfiles_mod.Runfiles = DummyRunfiles

        monkeypatch.setitem(sys.modules, "runfiles", runfiles_mod)
        assert _find_checklists_config() == str(cfg)

    def test_find_via_relative_fallback(self, monkeypatch):
        class DummyRunfiles:
            @staticmethod
            def Create():
                return None

        runfiles_mod = types.ModuleType("runfiles")
        runfiles_mod.Runfiles = DummyRunfiles

        monkeypatch.setitem(sys.modules, "runfiles", runfiles_mod)
        # Test with default config path (.github/review_checklists.yml)
        with patch("helpers.os.path.isfile", return_value=True):
            result = _find_checklists_config()
        assert result == ".github/review_checklists.yml"

    def test_find_via_custom_config_path(self, monkeypatch):
        class DummyRunfiles:
            @staticmethod
            def Create():
                return None

        runfiles_mod = types.ModuleType("runfiles")
        runfiles_mod.Runfiles = DummyRunfiles

        monkeypatch.setitem(sys.modules, "runfiles", runfiles_mod)
        # Test with custom config path
        with patch("helpers.os.path.isfile", return_value=True):
            result = _find_checklists_config("custom/checklists.yml")
        assert result == "custom/checklists.yml"


# ---------------------------------------------------------------------------
# merge-queue helpers
# ---------------------------------------------------------------------------


class TestIsPrInMergeQueue:
    @patch("helpers._run_graphql_query")
    def test_true_when_graphql_returns_true(self, mock_query):
        mock_query.return_value = {"data": {"repository": {"pullRequest": {"isInMergeQueue": True}}}}

        pr = MagicMock()
        pr.base.repo.full_name = "org/repo"
        pr.number = 42

        assert is_pr_in_merge_queue(pr) is True
        mock_query.assert_called_once()

    @patch("helpers._run_graphql_query")
    def test_false_when_graphql_returns_false(self, mock_query):
        mock_query.return_value = {"data": {"repository": {"pullRequest": {"isInMergeQueue": False}}}}

        pr = MagicMock()
        pr.base.repo.full_name = "org/repo"
        pr.number = 7

        assert is_pr_in_merge_queue(pr) is False

    @patch("helpers._run_graphql_query")
    def test_false_when_graphql_payload_missing_field(self, mock_query):
        mock_query.return_value = {"data": {"repository": {"pullRequest": {}}}}

        pr = MagicMock()
        pr.base.repo.full_name = "org/repo"
        pr.number = 99

        assert is_pr_in_merge_queue(pr) is False

    @patch("helpers._run_graphql_query")
    def test_false_when_graphql_call_fails(self, mock_query):
        mock_query.side_effect = RuntimeError("boom")

        pr = MagicMock()
        pr.base.repo.full_name = "org/repo"
        pr.number = 101

        assert is_pr_in_merge_queue(pr) is False


class TestEnsureMergeQueueNoticeDescription:
    def test_adds_notice_block(self):
        pr = MagicMock()
        pr.body = "User summary"

        ensure_merge_queue_notice_description(pr)

        pr.edit.assert_called_once()
        new_body = pr.edit.call_args.kwargs["body"]
        assert MERGE_QUEUE_NOTICE_START in new_body
        assert MERGE_QUEUE_NOTICE_END in new_body

    def test_updates_tampered_notice_block(self):
        pr = MagicMock()
        pr.body = f"User summary\n{MERGE_QUEUE_NOTICE_START}\ntampered\n{MERGE_QUEUE_NOTICE_END}"

        ensure_merge_queue_notice_description(pr)

        pr.edit.assert_called_once()
        new_body = pr.edit.call_args.kwargs["body"]
        assert "tampered" not in new_body
        assert "Review Checklist Evidence Notice - Merge Queue" in new_body

    def test_no_update_when_notice_already_present(self):
        pr = MagicMock()
        pr.body = "Intro"

        ensure_merge_queue_notice_description(pr)
        expected_body = pr.edit.call_args.kwargs["body"]
        pr.reset_mock()
        pr.body = expected_body

        ensure_merge_queue_notice_description(pr)

        pr.edit.assert_not_called()


class TestEnsureMergeQueueNoticeComment:
    def test_creates_comment_when_missing(self):
        pr = MagicMock()
        pr.get_issue_comments.return_value = []

        ensure_merge_queue_notice_comment(pr)

        pr.create_issue_comment.assert_called_once()
        posted = pr.create_issue_comment.call_args.args[0]
        assert MERGE_QUEUE_COMMENT_MARKER in posted

    def test_updates_existing_tampered_comment(self):
        existing = MagicMock()
        existing.body = f"{MERGE_QUEUE_COMMENT_MARKER}\nold text"

        pr = MagicMock()
        pr.get_issue_comments.return_value = [existing]

        ensure_merge_queue_notice_comment(pr)

        existing.edit.assert_called_once()
        updated = existing.edit.call_args.args[0]
        assert "merge queue" in updated.lower()

    def test_noop_when_existing_comment_matches(self):
        existing = MagicMock()
        existing.body = "\n".join([MERGE_QUEUE_COMMENT_MARKER] + MERGE_QUEUE_NOTICE)

        pr = MagicMock()
        pr.get_issue_comments.return_value = [existing]

        ensure_merge_queue_notice_comment(pr)

        existing.edit.assert_not_called()
        pr.create_issue_comment.assert_not_called()


if __name__ == "__main__":
    sys.exit(pytest.main(sys.argv[1:]))
