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

"""Unit tests for find_dependencies.py"""

import os
import tempfile
import unittest
from unittest.mock import MagicMock, patch

from find_dependencies import get_pr_body, main, parse_depends_on, write_github_output


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

    def test_mixed_formats(self):
        body = "Depends-On: #111\nDepends-On: 222\nDepends-On:#333"
        self.assertEqual(parse_depends_on(body), ["111", "222", "333"])

    def test_case_insensitive_lower(self):
        self.assertEqual(parse_depends_on("depends-on: #555"), ["555"])

    def test_case_insensitive_upper(self):
        self.assertEqual(parse_depends_on("DEPENDS-ON: #666"), ["666"])

    def test_case_insensitive_mixed(self):
        self.assertEqual(parse_depends_on("DePeNdS-On: #777"), ["777"])

    def test_no_dependencies(self):
        self.assertEqual(parse_depends_on("No dependencies here."), [])

    def test_empty_body(self):
        self.assertEqual(parse_depends_on(""), [])

    def test_extra_whitespace(self):
        self.assertEqual(parse_depends_on("Depends-On:    #888"), ["888"])

    def test_markdown_list(self):
        body = "## Dependencies\n- Depends-On: #999\n- Depends-On: #1000"
        self.assertEqual(parse_depends_on(body), ["999", "1000"])

    def test_surrounding_text(self):
        self.assertEqual(
            parse_depends_on("Please merge Depends-On: #1111 first before this one"), ["1111"]
        )

    def test_large_pr_numbers(self):
        self.assertEqual(parse_depends_on("Depends-On: #99999"), ["99999"])

    def test_partial_no_number(self):
        self.assertEqual(parse_depends_on("Depends-On:"), [])

    def test_number_before_keyword(self):
        self.assertEqual(parse_depends_on("123 Depends-On"), [])


# ---------------------------------------------------------------------------
# Tests for write_github_output
# ---------------------------------------------------------------------------


class TestWriteGithubOutput(unittest.TestCase):
    def _read_output(self, prs: list[str]) -> str:
        with tempfile.NamedTemporaryFile(mode="w", suffix=".txt", delete=False) as f:
            path = f.name
        write_github_output(path, prs)
        with open(path, encoding="utf-8") as f:
            return f.read()

    def test_writes_prs_and_count(self):
        content = self._read_output(["100", "200", "300"])
        self.assertIn("dependency_prs=100,200,300\n", content)
        self.assertIn("dependency_count=3\n", content)

    def test_writes_single_pr(self):
        content = self._read_output(["42"])
        self.assertIn("dependency_prs=42\n", content)
        self.assertIn("dependency_count=1\n", content)

    def test_writes_empty(self):
        content = self._read_output([])
        self.assertIn("dependency_prs=\n", content)
        self.assertIn("dependency_count=0\n", content)

    def test_appends_to_existing_content(self):
        with tempfile.NamedTemporaryFile(mode="w", suffix=".txt", delete=False) as f:
            f.write("existing=value\n")
            path = f.name
        write_github_output(path, ["7"])
        with open(path, encoding="utf-8") as f:
            content = f.read()
        self.assertIn("existing=value\n", content)
        self.assertIn("dependency_prs=7\n", content)


# ---------------------------------------------------------------------------
# Tests for get_pr_body
# ---------------------------------------------------------------------------


class TestGetPrBody(unittest.TestCase):
    def _mock_run(self, returncode: int, stdout: str) -> MagicMock:
        m = MagicMock()
        m.returncode = returncode
        m.stdout = stdout
        return m

    def test_returns_pr_body(self):
        with patch("find_dependencies.subprocess.run", return_value=self._mock_run(0, "Depends-On: #123\n")):
            result = get_pr_body("42")
        self.assertEqual(result, "Depends-On: #123")

    def test_strips_trailing_newline(self):
        with patch("find_dependencies.subprocess.run", return_value=self._mock_run(0, "body text\n\n")):
            result = get_pr_body("1")
        self.assertEqual(result, "body text")

    def test_raises_on_gh_failure(self):
        with patch("find_dependencies.subprocess.run", return_value=self._mock_run(1, "")):
            with self.assertRaises(RuntimeError):
                get_pr_body("999")

    def test_exception_message_contains_pr_number_and_exit_code(self):
        mock = self._mock_run(2, "")
        mock.stderr = "authentication required"
        with patch("find_dependencies.subprocess.run", return_value=mock):
            with self.assertRaises(RuntimeError) as ctx:
                get_pr_body("42")
        self.assertIn("42", str(ctx.exception))
        self.assertIn("2", str(ctx.exception))
        self.assertIn("authentication required", str(ctx.exception))


# ---------------------------------------------------------------------------
# Tests for main
# ---------------------------------------------------------------------------


class TestMain(unittest.TestCase):
    def _run_main(self, env: dict, pr_body: str = "") -> tuple[int, str]:
        """Run main() with given env overrides; return (exit_code, github_output_content)."""
        with tempfile.NamedTemporaryFile(mode="w", suffix=".txt", delete=False) as f:
            output_path = f.name

        mock_result = MagicMock()
        mock_result.returncode = 0
        mock_result.stdout = pr_body

        with patch.dict(os.environ, {**env, "GITHUB_OUTPUT": output_path}, clear=False):
            with patch("find_dependencies.subprocess.run", return_value=mock_result):
                exit_code = main()

        with open(output_path, encoding="utf-8") as f:
            return exit_code, f.read()

    def test_non_pr_event_skips(self):
        code, output = self._run_main({"EVENT_NAME": "push", "PR_NUMBER": ""})
        self.assertEqual(code, 0)
        self.assertIn("dependency_prs=\n", output)
        self.assertIn("dependency_count=0\n", output)

    def test_pull_request_event_runs(self):
        code, output = self._run_main(
            {"EVENT_NAME": "pull_request", "PR_NUMBER": "42"},
            pr_body="Depends-On: #100",
        )
        self.assertEqual(code, 0)
        self.assertIn("dependency_prs=100\n", output)
        self.assertIn("dependency_count=1\n", output)

    def test_merge_group_event_runs(self):
        code, output = self._run_main(
            {"EVENT_NAME": "merge_group", "PR_NUMBER": "123"},
            pr_body="Depends-On: #456",
        )
        self.assertEqual(code, 0)
        self.assertIn("dependency_prs=456\n", output)
        self.assertIn("dependency_count=1\n", output)

    def test_missing_pr_number_skips(self):
        code, output = self._run_main({"EVENT_NAME": "pull_request", "PR_NUMBER": ""})
        self.assertEqual(code, 0)
        self.assertIn("dependency_prs=\n", output)
        self.assertIn("dependency_count=0\n", output)

    def test_gh_failure_returns_exit_1(self):
        failing_mock = MagicMock()
        failing_mock.returncode = 1
        failing_mock.stdout = ""
        failing_mock.stderr = "authentication required"
        with tempfile.NamedTemporaryFile(mode="w", suffix=".txt", delete=False) as f:
            output_path = f.name
        with patch.dict(os.environ, {"EVENT_NAME": "pull_request", "PR_NUMBER": "42", "GITHUB_OUTPUT": output_path}, clear=False):
            with patch("find_dependencies.subprocess.run", return_value=failing_mock):
                code = main()
        with open(output_path, encoding="utf-8") as f:
            output = f.read()
        self.assertEqual(code, 1)
        self.assertIn("dependency_prs=\n", output)
        self.assertIn("dependency_count=0\n", output)

    def test_empty_pr_body_skips(self):
        code, output = self._run_main(
            {"EVENT_NAME": "pull_request", "PR_NUMBER": "42"},
            pr_body="",
        )
        self.assertEqual(code, 0)
        self.assertIn("dependency_prs=\n", output)
        self.assertIn("dependency_count=0\n", output)

    def test_pr_with_no_depends_on(self):
        code, output = self._run_main(
            {"EVENT_NAME": "pull_request", "PR_NUMBER": "42"},
            pr_body="Just a regular PR description.",
        )
        self.assertEqual(code, 0)
        self.assertIn("dependency_prs=\n", output)
        self.assertIn("dependency_count=0\n", output)

    def test_multiple_dependencies(self):
        code, output = self._run_main(
            {"EVENT_NAME": "pull_request", "PR_NUMBER": "42"},
            pr_body="Depends-On: #100\nDepends-On: #200",
        )
        self.assertEqual(code, 0)
        self.assertIn("dependency_prs=100,200\n", output)
        self.assertIn("dependency_count=2\n", output)


if __name__ == "__main__":
    unittest.main()

