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

"""Unit tests for rebase_on_prs.py"""

import os
import tempfile
import unittest
from unittest.mock import MagicMock, patch

from rebase_on_prs import (
    REBASE_FAILED,
    REBASE_SKIPPED,
    REBASE_SUCCESS,
    get_pr_field,
    main,
    rebase_on_pr,
    write_github_output,
)


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _completed(returncode: int = 0, stdout: str = "") -> MagicMock:
    m = MagicMock()
    m.returncode = returncode
    m.stdout = stdout
    return m


def _gh_side_effect(branch: str = "feature-branch", state: str = "OPEN"):
    """Return a subprocess.run side-effect that mocks gh pr view calls."""

    def side_effect(cmd, **kwargs):
        if cmd[0] == "gh":
            field = cmd[5]  # ["gh", "pr", "view", pr, "--json", field, "--jq", ...]
            if field == "headRefName":
                return _completed(0, f"{branch}\n")
            if field == "state":
                return _completed(0, f"{state}\n")
        # git calls (fetch, rebase, config) succeed by default
        return _completed(0, "")

    return side_effect


# ---------------------------------------------------------------------------
# Tests for get_pr_field
# ---------------------------------------------------------------------------


class TestGetPrField(unittest.TestCase):
    def test_returns_branch_name(self):
        with patch("rebase_on_prs.subprocess.run", return_value=_completed(0, "feature-branch\n")):
            self.assertEqual(get_pr_field("123", "headRefName"), "feature-branch")

    def test_strips_trailing_newline(self):
        with patch("rebase_on_prs.subprocess.run", return_value=_completed(0, "OPEN\n")):
            self.assertEqual(get_pr_field("123", "state"), "OPEN")

    def test_returns_empty_on_gh_failure(self):
        with patch("rebase_on_prs.subprocess.run", return_value=_completed(1, "")):
            self.assertEqual(get_pr_field("999", "headRefName"), "")


# ---------------------------------------------------------------------------
# Tests for rebase_on_pr
# ---------------------------------------------------------------------------


class TestRebaseOnPr(unittest.TestCase):
    def test_open_pr_successful_rebase(self):
        with patch("rebase_on_prs.subprocess.run", side_effect=_gh_side_effect("my-branch", "OPEN")):
            self.assertEqual(rebase_on_pr("123"), REBASE_SUCCESS)

    def test_merged_pr_returns_skipped(self):
        with patch("rebase_on_prs.subprocess.run", side_effect=_gh_side_effect("my-branch", "MERGED")):
            self.assertEqual(rebase_on_pr("123"), REBASE_SKIPPED)

    def test_closed_pr_returns_skipped(self):
        with patch("rebase_on_prs.subprocess.run", side_effect=_gh_side_effect("my-branch", "CLOSED")):
            self.assertEqual(rebase_on_pr("123"), REBASE_SKIPPED)

    def test_pr_not_found_returns_failed(self):
        # gh returns an empty string for the branch name
        with patch("rebase_on_prs.subprocess.run", return_value=_completed(0, "\n")):
            self.assertEqual(rebase_on_pr("999"), REBASE_FAILED)

    def test_git_fetch_failure_returns_failed(self):
        def side_effect(cmd, **kwargs):
            if cmd[0] == "gh":
                field = cmd[5]
                if field == "headRefName":
                    return _completed(0, "feature-branch\n")
                if field == "state":
                    return _completed(0, "OPEN\n")
            if cmd[0] == "git" and cmd[1] == "fetch":
                return _completed(1)  # fetch fails
            return _completed(0)

        with patch("rebase_on_prs.subprocess.run", side_effect=side_effect):
            self.assertEqual(rebase_on_pr("123"), REBASE_FAILED)

    def test_rebase_conflict_returns_failed(self):
        def side_effect(cmd, **kwargs):
            if cmd[0] == "gh":
                field = cmd[5]
                if field == "headRefName":
                    return _completed(0, "feature-branch\n")
                if field == "state":
                    return _completed(0, "OPEN\n")
            if cmd[0] == "git":
                if cmd[1] == "rebase" and (len(cmd) < 3 or cmd[2] != "--abort"):
                    return _completed(1)  # rebase fails with conflict
                return _completed(0)  # fetch and rebase --abort succeed
            return _completed(0)

        with patch("rebase_on_prs.subprocess.run", side_effect=side_effect):
            self.assertEqual(rebase_on_pr("123"), REBASE_FAILED)

    def test_rebase_conflict_calls_abort(self):
        abort_called = []

        def side_effect(cmd, **kwargs):
            if cmd[0] == "gh":
                field = cmd[5]
                if field == "headRefName":
                    return _completed(0, "feature-branch\n")
                if field == "state":
                    return _completed(0, "OPEN\n")
            if cmd[0] == "git":
                if cmd[1] == "rebase" and cmd[2:] == ["--abort"]:
                    abort_called.append(True)
                    return _completed(0)
                if cmd[1] == "rebase":
                    return _completed(1)
                return _completed(0)
            return _completed(0)

        with patch("rebase_on_prs.subprocess.run", side_effect=side_effect):
            rebase_on_pr("123")

        self.assertTrue(abort_called, "git rebase --abort should have been called")

    def test_fetch_uses_pr_ref_not_branch_name(self):
        """Verify the exact git fetch refspec: refs/pull/<pr>/head → local ref (not the branch name).

        This is what makes fork PRs work — the branch name lives on a different
        repository, but GitHub always exposes refs/pull/<pr>/head on the base repo.
        """
        fetch_cmds: list[list[str]] = []

        def side_effect(cmd, **kwargs):
            if cmd[0] == "gh":
                field = cmd[5]
                if field == "headRefName":
                    return _completed(0, "feature-branch\n")
                if field == "state":
                    return _completed(0, "OPEN\n")
            if cmd[0] == "git" and cmd[1] == "fetch":
                fetch_cmds.append(list(cmd))
            return _completed(0)

        with patch("rebase_on_prs.subprocess.run", side_effect=side_effect):
            result = rebase_on_pr("42")

        self.assertEqual(result, REBASE_SUCCESS)
        self.assertEqual(len(fetch_cmds), 1)
        self.assertEqual(
            fetch_cmds[0],
            ["git", "fetch", "origin", "refs/pull/42/head:refs/remotes/origin/pr/42"],
        )


# ---------------------------------------------------------------------------
# Tests for write_github_output
# ---------------------------------------------------------------------------


class TestWriteGithubOutput(unittest.TestCase):
    def _read(self, **kwargs) -> str:
        with tempfile.NamedTemporaryFile(mode="w", suffix=".txt", delete=False) as f:
            path = f.name
        write_github_output(path, **kwargs)
        with open(path, encoding="utf-8") as f:
            return f.read()

    def test_successful_rebase(self):
        content = self._read(rebased=True, rebased_count=2, skipped_prs=[], failed_prs=[])
        self.assertIn("rebased=true\n", content)
        self.assertIn("rebased_count=2\n", content)
        self.assertIn("skipped_prs=\n", content)
        self.assertIn("failed_prs=\n", content)

    def test_no_rebase_needed(self):
        content = self._read(rebased=False, rebased_count=0, skipped_prs=["123"], failed_prs=[])
        self.assertIn("rebased=false\n", content)
        self.assertIn("rebased_count=0\n", content)
        self.assertIn("skipped_prs=123\n", content)

    def test_failed_prs(self):
        content = self._read(rebased=False, rebased_count=0, skipped_prs=[], failed_prs=["42", "99"])
        self.assertIn("failed_prs=42,99\n", content)

    def test_appends_to_existing_content(self):
        with tempfile.NamedTemporaryFile(mode="w", suffix=".txt", delete=False) as f:
            f.write("existing=value\n")
            path = f.name
        write_github_output(path, rebased=True, rebased_count=1, skipped_prs=[], failed_prs=[])
        with open(path, encoding="utf-8") as f:
            content = f.read()
        self.assertIn("existing=value\n", content)
        self.assertIn("rebased=true\n", content)


# ---------------------------------------------------------------------------
# Tests for main
# ---------------------------------------------------------------------------


class TestMain(unittest.TestCase):
    def _run_main(self, dependency_prs_str: str, side_effect) -> tuple[int, str]:
        """Run main() with a subprocess mock; return (exit_code, github_output_content)."""
        with tempfile.NamedTemporaryFile(mode="w", suffix=".txt", delete=False) as f:
            output_path = f.name

        with patch.dict(os.environ, {"GITHUB_OUTPUT": output_path}, clear=False):
            with patch("rebase_on_prs.subprocess.run", side_effect=side_effect):
                exit_code = main(dependency_prs_str)

        with open(output_path, encoding="utf-8") as f:
            return exit_code, f.read()

    def test_single_successful_rebase(self):
        code, output = self._run_main("123", _gh_side_effect("feature-branch", "OPEN"))
        self.assertEqual(code, 0)
        self.assertIn("rebased=true\n", output)
        self.assertIn("rebased_count=1\n", output)
        self.assertIn("skipped_prs=\n", output)
        self.assertIn("failed_prs=\n", output)

    def test_multiple_successful_rebases(self):
        code, output = self._run_main("111,222", _gh_side_effect("feature-branch", "OPEN"))
        self.assertEqual(code, 0)
        self.assertIn("rebased=true\n", output)
        self.assertIn("rebased_count=2\n", output)

    def test_all_prs_merged_no_rebase(self):
        code, output = self._run_main("123,456", _gh_side_effect("feature-branch", "MERGED"))
        self.assertEqual(code, 0)
        self.assertIn("rebased=false\n", output)
        self.assertIn("rebased_count=0\n", output)
        self.assertIn("skipped_prs=123,456\n", output)

    def test_pr_not_found_returns_error(self):
        code, output = self._run_main("999", lambda cmd, **kw: _completed(0, "\n"))
        self.assertEqual(code, 1)
        self.assertIn("failed_prs=999\n", output)
        self.assertIn("rebased=false\n", output)

    def test_mixed_open_and_merged(self):
        states = {"111": "OPEN", "222": "MERGED"}

        def side_effect(cmd, **kwargs):
            if cmd[0] == "gh":
                pr = cmd[3]
                field = cmd[5]
                if field == "headRefName":
                    return _completed(0, "feature-branch\n")
                if field == "state":
                    return _completed(0, f"{states.get(pr, 'OPEN')}\n")
            return _completed(0)

        code, output = self._run_main("111,222", side_effect)
        self.assertEqual(code, 0)
        self.assertIn("rebased_count=1\n", output)
        self.assertIn("skipped_prs=222\n", output)
        self.assertIn("failed_prs=\n", output)

    def test_one_failed_pr_returns_exit_1(self):
        states = {"100": "OPEN", "200": "OPEN"}

        def side_effect(cmd, **kwargs):
            if cmd[0] == "gh":
                pr = cmd[3]
                field = cmd[5]
                if field == "headRefName":
                    return _completed(0, f"branch-{pr}\n")
                if field == "state":
                    return _completed(0, f"{states.get(pr, 'OPEN')}\n")
            if cmd[0] == "git":
                if cmd[1] == "rebase" and cmd[2:] != ["--abort"]:
                    # Fail the rebase when targeting PR 200's local ref
                    if "pr/200" in cmd[2]:
                        return _completed(1)
                return _completed(0)
            return _completed(0)

        code, output = self._run_main("100,200", side_effect)
        self.assertEqual(code, 1)
        self.assertIn("rebased_count=1\n", output)
        self.assertIn("failed_prs=200\n", output)

    def test_fork_pr_succeeds(self):
        """A fork PR (branch not on origin) still succeeds because refs/pull/<pr>/head is used."""
        # The branch name coming from gh would normally be on the fork owner's repo.
        # With the old branch-name-based fetch this would fail; the new approach always
        # fetches via refs/pull/<pr>/head from origin, which GitHub exposes for every PR.
        code, output = self._run_main("99", _gh_side_effect("some-fork-user:my-feature", "OPEN"))
        self.assertEqual(code, 0)
        self.assertIn("rebased=true\n", output)
        self.assertIn("rebased_count=1\n", output)

    def test_fetch_uses_pr_ref_not_branch_name(self):
        """Verify the exact git fetch refspec is refs/pull/<pr>/head:<local_ref>."""
        fetch_cmds: list[list[str]] = []

        def side_effect(cmd, **kwargs):
            if cmd[0] == "gh":
                field = cmd[5]
                if field == "headRefName":
                    return _completed(0, "feature-branch\n")
                if field == "state":
                    return _completed(0, "OPEN\n")
            if cmd[0] == "git" and cmd[1] == "fetch":
                fetch_cmds.append(list(cmd))
            return _completed(0)

        self._run_main("42", side_effect)

        self.assertEqual(len(fetch_cmds), 1)
        self.assertEqual(
            fetch_cmds[0],
            ["git", "fetch", "origin", "refs/pull/42/head:refs/remotes/origin/pr/42"],
        )


if __name__ == "__main__":
    unittest.main()

