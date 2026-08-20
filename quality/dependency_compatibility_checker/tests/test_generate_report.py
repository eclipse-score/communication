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
import tempfile
import textwrap
import unittest
from pathlib import Path

from quality.dependency_compatibility_checker import generate_report


def _config() -> str:
    path = Path(tempfile.mkdtemp()) / "config.yaml"
    path.write_text(
        textwrap.dedent(
            """
        dependencies:
          bazel:
            versions: [{version: "8.7.0"}]
          rules_cc:
            versions:
              - version: "0.2.17"
              - version: "0.2.21"
                skip: "broken, see #700"
        """
        ),
        encoding="utf-8",
    )
    return str(path)


def _results(*results) -> str:
    d = Path(tempfile.mkdtemp())
    for r in results:
        (d / f"result-{r['index']}.json").write_text(json.dumps(r), encoding="utf-8")
    return str(d)


class ClassifyTest(unittest.TestCase):
    def test_classification(self):
        self.assertEqual(generate_report.classify("success", False), "green")
        self.assertEqual(generate_report.classify("success", True), "orange")
        self.assertEqual(generate_report.classify("failure", False), "red")
        self.assertEqual(generate_report.classify("failure", True), "red")


class ReportTest(unittest.TestCase):
    def _run(self, fail_on, *results):
        out = Path(tempfile.mkdtemp())
        rc = generate_report.main(
            [
                "--results-dir",
                _results(*results),
                "--config",
                _config(),
                "--output-dir",
                str(out),
                "--fail-on",
                fail_on,
            ]
        )
        summary = json.loads((out / "summary.json").read_text())
        html = (out / "report.html").read_text()
        return rc, summary, html

    def test_summary_and_html_written(self):
        rc, summary, html = self._run(
            "never",
            {
                "index": 0,
                "versions": {"bazel": "8.7.0", "rules_cc": "0.2.17"},
                "build_status": "success",
                "has_patches": False,
            },
        )
        self.assertEqual(rc, 0)
        self.assertEqual(summary["totals"], {"green": 1, "orange": 0, "red": 0})
        self.assertEqual(summary["skipped"][0]["version"], "0.2.21")
        self.assertIn("green", html.lower())

    def test_fail_on_red(self):
        rc, _, _ = self._run(
            "red",
            {
                "index": 0,
                "versions": {"bazel": "8.7.0", "rules_cc": "0.2.17"},
                "build_status": "failure",
                "has_patches": False,
            },
        )
        self.assertNotEqual(rc, 0)

    def test_fail_on_never_ignores_red(self):
        rc, _, _ = self._run(
            "never",
            {
                "index": 0,
                "versions": {"bazel": "8.7.0", "rules_cc": "0.2.17"},
                "build_status": "failure",
                "has_patches": False,
            },
        )
        self.assertEqual(rc, 0)


if __name__ == "__main__":
    unittest.main()
