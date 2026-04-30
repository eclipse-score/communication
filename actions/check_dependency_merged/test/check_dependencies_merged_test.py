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

"""Unit tests for check_dependencies_merged.py"""

import os
import tempfile
import unittest
from unittest.mock import MagicMock, patch

from check_dependencies_merged import (
    check_dependencies,
    get_pr_body,
    get_pr_state,
    main,
    parse_depends_on,
    write_github_output,
)


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _completed(returncode: int = 0, stdout: str = "", stderr: str = "") -> MagicMock:
    m = MagicMock()
    m.returncode = returncode
    m.stdout = stdout
    m.stderr = stderr
    return m


def _gh_state_side_effect(states: dict[str, str]):
    """Return a subprocess.run side-effect that returns configured PR states."""

    def side_effect(cmd, **kwargs):
        if cmd[0] == "gh":
            pr = cmd[3]
            return _completed(0, f"{states.get(pr, 'OPEN')}\n")
        return _completed(0)

    return side_effect


# ---------------------------------------------------------------------------
# Tests for parse_depends_on
# ---------------------------------------------------------------------------


class TestParseDependsOn(unittest.TestCase):
    def test_single_dependency_with_hash(self):
        body = "This PR depends on another one.\n\nDepends-On: #123\n\nMore description."
        self.assertEqual(parse_depends_on(body), ["123"])

    def test_single_dependency_without_hash(self):
        self.assertEqual(parse_depends_on("Depends-On: 456"), ["456"])

    def test_single_dependency_no_space(self):
        self.assertEqual(parse_depends_on("Depends-On:#789"), ["789"])

    def test_multiple_dependencies(self):
        body = "Depends-On: #100\nDepends-On: #200\nDepends-On: #300"
        self.assertEqual(parse_depends_on(body), ["100", "200", "300"])

    def test_case_insensitive(self):
        self.assertEqual(parse_depends_on("depends-on: #555"), ["555"])
        self.assertEqual(parse_depends_on("DEPENDS-ON: #666"), ["666"])

    def test_no_dependencies(self):
        self.assertEqual(parse_depends_on("No dependencies here."), [])

    def test_empty_body(self):
        self.assertEqual(parse_depends_on(""), [])


# ---------------------------------------------------------------------------
# Tests for get_pr_body
# ---------------------------------------------------------------------------


class TestGetPrBody(unittest.TestCase):
    def test_returns_pr_body(self):
        with patch(
            "check_dependencies_merged.subprocess.run",
            return_value=_completed(0, "Depends-On: #123\n"),
        ):
            self.assertEqual(get_pr_body("42"), "Depends-On: #123")

    def test_raises_on_gh_failure(self):
        with patch(
            "check_dependencies_merged.subprocess.run",
            return_value=_completed(1, "", "auth required"),
        ):
            with self.assertRaises(RuntimeError):
                get_pr_body("999")


# ---------------------------------------------------------------------------
# Tests for get_pr_state
# ---------------------------------------------------------------------------


class TestGetPrState(unittest.TestCase):
    def test_returns_merged(self):
        with patch(
            "check_dependencies_merged.subprocess.run",
            return_value=_completed(0, "MERGED\n"),
        ):
            self.assertEqual(get_pr_state("123"), "MERGED")

    def test_returns_open(self):
        with patch(
            "check_dependencies_merged.subprocess.run",
            return_value=_completed(0, "OPEN\n"),
        ):
            self.assertEqual(get_pr_state("123"), "OPEN")

    def test_returns_closed(self):
        with patch(
            "check_dependencies_merged.subprocess.run",
            return_value=_completed(0, "CLOSED\n"),
        ):
            self.assertEqual(get_pr_state("123"), "CLOSED")

    def test_strips_trailing_newline(self):
        with patch(
            "check_dependencies_merged.subprocess.run",
            return_value=_completed(0, "MERGED\n\n"),
        ):
            self.assertEqual(get_pr_state("42"), "MERGED")

    def test_returns_empty_on_gh_failure(self):
        with patch(
            "check_dependencies_merged.subprocess.run",
            return_value=_completed(1, ""),
        ):
            self.assertEqual(get_pr_state("999"), "")


# ---------------------------------------------------------------------------
# Tests for check_dependencies
# ---------------------------------------------------------------------------


class TestCheckDependencies(unittest.TestCase):
    def test_all_merged(self):
        with patch(
            "check_dependencies_merged.subprocess.run",
            side_effect=_gh_state_side_effect({"100": "MERGED", "200": "MERGED"}),
        ):
            all_merged, unmerged = check_dependencies(["100", "200"])
        self.assertTrue(all_merged)
        self.assertEqual(unmerged, [])

    def test_one_open(self):
        with patch(
            "check_dependencies_merged.subprocess.run",
            side_effect=_gh_state_side_effect({"100": "MERGED", "200": "OPEN"}),
        ):
            all_merged, unmerged = check_dependencies(["100", "200"])
        self.assertFalse(all_merged)
        self.assertEqual(unmerged, ["200"])

    def test_all_open(self):
        with patch(
            "check_dependencies_merged.subprocess.run",
            side_effect=_gh_state_side_effect({"100": "OPEN", "200": "OPEN"}),
        ):
            all_merged, unmerged = check_dependencies(["100", "200"])
        self.assertFalse(all_merged)
        self.assertEqual(unmerged, ["100", "200"])

    def test_closed_not_merged_counts_as_unmerged(self):
        with patch(
            "check_dependencies_merged.subprocess.run",
            side_effect=_gh_state_side_effect({"100": "CLOSED"}),
        ):
            all_merged, unmerged = check_dependencies(["100"])
        self.assertFalse(all_merged)
        self.assertEqual(unmerged, ["100"])

    def test_unknown_state_counts_as_unmerged(self):
        with patch(
            "check_dependencies_merged.subprocess.run",
            return_value=_completed(1, ""),
        ):
            all_merged, unmerged = check_dependencies(["999"])
        self.assertFalse(all_merged)
        self.assertEqual(unmerged, ["999"])

    def test_single_merged(self):
        with patch(
            "check_dependencies_merged.subprocess.run",
            side_effect=_gh_state_side_effect({"42": "MERGED"}),
        ):
            all_merged, unmerged = check_dependencies(["42"])
        self.assertTrue(all_merged)
        self.assertEqual(unmerged, [])

    def test_mixed_merged_open_closed(self):
        states = {"10": "MERGED", "20": "OPEN", "30": "CLOSED"}
        with patch(
            "check_dependencies_merged.subprocess.run",
            side_effect=_gh_state_side_effect(states),
        ):
            all_merged, unmerged = check_dependencies(["10", "20", "30"])
        self.assertFalse(all_merged)
        self.assertEqual(unmerged, ["20", "30"])


# ---------------------------------------------------------------------------
# Tests for write_github_output
# ---------------------------------------------------------------------------


class TestWriteGithubOutput(unittest.TestCase):
    def _read(self, dependency_prs: list[str], all_merged: bool, unmerged_prs: list[str]) -> str:
        with tempfile.NamedTemporaryFile(mode="w", suffix=".txt", delete=False) as f:
            path = f.name
        write_github_output(path, dependency_prs, all_merged, unmerged_prs)
        with open(path, encoding="utf-8") as f:
            return f.read()

    def test_all_merged(self):
        content = self._read(["100", "200"], True, [])
        self.assertIn("dependency_prs=100,200\n", content)
        self.assertIn("dependency_count=2\n", content)
        self.assertIn("all_dependencies_merged=true\n", content)
        self.assertIn("unmerged_prs=\n", content)

    def test_some_unmerged(self):
        content = self._read(["100", "200"], False, ["200"])
        self.assertIn("all_dependencies_merged=false\n", content)
        self.assertIn("unmerged_prs=200\n", content)

    def test_no_dependencies(self):
        content = self._read([], True, [])
        self.assertIn("dependency_prs=\n", content)
        self.assertIn("dependency_count=0\n", content)
        self.assertIn("all_dependencies_merged=true\n", content)

    def test_appends_to_existing_content(self):
        with tempfile.NamedTemporaryFile(mode="w", suffix=".txt", delete=False) as f:
            f.write("existing=value\n")
            path = f.name
        write_github_output(path, ["7"], True, [])
        with open(path, encoding="utf-8") as f:
            content = f.read()
        self.assertIn("existing=value\n", content)
        self.assertIn("all_dependencies_merged=true\n", content)


# ---------------------------------------------------------------------------
# Tests for main
# ---------------------------------------------------------------------------


class TestMain(unittest.TestCase):
    def _mock_gh(self, pr_body: str = "", states: dict[str, str] | None = None):
        """Return a subprocess.run side-effect for both body and state queries."""
        if states is None:
            states = {}

        def side_effect(cmd, **kwargs):
            if cmd[0] != "gh":
                return _completed(0)
            # cmd: ["gh", "pr", "view", pr, "--json", field, "--jq", ...]
            field = cmd[5]
            pr = cmd[3]
            if field == "body":
                return _completed(0, f"{pr_body}\n")
            if field == "state":
                return _completed(0, f"{states.get(pr, 'OPEN')}\n")
            return _completed(0)

        return side_effect

    def _run_main(self, env: dict, side_effect) -> tuple[int, str]:
        """Run main() with given env and subprocess mock; return (exit_code, output_content)."""
        with tempfile.NamedTemporaryFile(mode="w", suffix=".txt", delete=False) as f:
            output_path = f.name

        with patch.dict(os.environ, {**env, "GITHUB_OUTPUT": output_path}, clear=False):
            with patch("check_dependencies_merged.subprocess.run", side_effect=side_effect):
                exit_code = main()

        with open(output_path, encoding="utf-8") as f:
            return exit_code, f.read()

    def test_non_pr_event_skips(self):
        code, output = self._run_main(
            {"EVENT_NAME": "push", "PR_NUMBER": ""},
            self._mock_gh(),
        )
        self.assertEqual(code, 0)
        self.assertIn("all_dependencies_merged=true\n", output)

    def test_no_pr_number_skips(self):
        code, output = self._run_main(
            {"EVENT_NAME": "pull_request", "PR_NUMBER": ""},
            self._mock_gh(),
        )
        self.assertEqual(code, 0)
        self.assertIn("all_dependencies_merged=true\n", output)

    def test_no_depends_on_passes(self):
        code, output = self._run_main(
            {"EVENT_NAME": "pull_request", "PR_NUMBER": "42"},
            self._mock_gh(pr_body="Just a regular PR"),
        )
        self.assertEqual(code, 0)
        self.assertIn("all_dependencies_merged=true\n", output)
        self.assertIn("dependency_prs=\n", output)

    def test_all_deps_merged_passes(self):
        code, output = self._run_main(
            {"EVENT_NAME": "pull_request", "PR_NUMBER": "42"},
            self._mock_gh(
                pr_body="Depends-On: #100\nDepends-On: #200",
                states={"100": "MERGED", "200": "MERGED"},
            ),
        )
        self.assertEqual(code, 0)
        self.assertIn("all_dependencies_merged=true\n", output)
        self.assertIn("dependency_prs=100,200\n", output)

    def test_unmerged_dep_fails(self):
        code, output = self._run_main(
            {"EVENT_NAME": "pull_request", "PR_NUMBER": "42"},
            self._mock_gh(
                pr_body="Depends-On: #100\nDepends-On: #200",
                states={"100": "MERGED", "200": "OPEN"},
            ),
        )
        self.assertEqual(code, 1)
        self.assertIn("all_dependencies_merged=false\n", output)
        self.assertIn("unmerged_prs=200\n", output)

    def test_single_open_dep_fails(self):
        code, output = self._run_main(
            {"EVENT_NAME": "pull_request", "PR_NUMBER": "42"},
            self._mock_gh(pr_body="Depends-On: #99", states={"99": "OPEN"}),
        )
        self.assertEqual(code, 1)
        self.assertIn("unmerged_prs=99\n", output)

    def test_single_merged_dep_passes(self):
        code, output = self._run_main(
            {"EVENT_NAME": "pull_request", "PR_NUMBER": "42"},
            self._mock_gh(pr_body="Depends-On: #99", states={"99": "MERGED"}),
        )
        self.assertEqual(code, 0)
        self.assertIn("all_dependencies_merged=true\n", output)

    def test_closed_dep_counts_as_unmerged(self):
        code, output = self._run_main(
            {"EVENT_NAME": "pull_request", "PR_NUMBER": "42"},
            self._mock_gh(pr_body="Depends-On: #50", states={"50": "CLOSED"}),
        )
        self.assertEqual(code, 1)
        self.assertIn("unmerged_prs=50\n", output)

    def test_merge_group_event_is_supported(self):
        code, output = self._run_main(
            {"EVENT_NAME": "merge_group", "PR_NUMBER": "42"},
            self._mock_gh(pr_body="Depends-On: #10", states={"10": "MERGED"}),
        )
        self.assertEqual(code, 0)
        self.assertIn("all_dependencies_merged=true\n", output)

    def test_empty_body_passes(self):
        code, output = self._run_main(
            {"EVENT_NAME": "pull_request", "PR_NUMBER": "42"},
            self._mock_gh(pr_body=""),
        )
        self.assertEqual(code, 0)
        self.assertIn("all_dependencies_merged=true\n", output)

    def test_gh_body_failure_returns_exit_1(self):
        failing_mock = _completed(1, "", "auth required")

        code, output = self._run_main(
            {"EVENT_NAME": "pull_request", "PR_NUMBER": "42"},
            lambda cmd, **kw: failing_mock,
        )
        self.assertEqual(code, 1)
        self.assertIn("all_dependencies_merged=true\n", output)


if __name__ == "__main__":
    unittest.main()

