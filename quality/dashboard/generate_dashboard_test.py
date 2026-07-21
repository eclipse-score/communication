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
import importlib.util
import os
import pathlib
import sys
import tempfile
import unittest


_MODULE_PATH = pathlib.Path(__file__).with_name("generate_dashboard.py")
_SPEC = importlib.util.spec_from_file_location("generate_dashboard", _MODULE_PATH)
assert _SPEC is not None and _SPEC.loader is not None
generate_dashboard = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(generate_dashboard)


class GenerateDashboardTest(unittest.TestCase):
    def test_load_linter_findings_reads_sarif_levels(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            sarif_path = pathlib.Path(tmp_dir) / "clang-tidy.sarif"
            sarif_path.write_text(
                json.dumps(
                    {
                        "$schema": "https://docs.oasis-open.org/sarif/sarif/v2.1.0/errata01/os/schemas/sarif-schema-2.1.0.json",
                        "version": "2.1.0",
                        "runs": [
                            {
                                "tool": {
                                    "driver": {
                                        "name": "ClangTidy",
                                    },
                                },
                                "results": [
                                    {
                                        "level": "warning",
                                        "message": {"text": "unused variable"},
                                        "locations": [
                                            {
                                                "physicalLocation": {
                                                    "artifactLocation": {
                                                        "uri": "./src/foo.cc",
                                                        "uriBaseId": "%SRCROOT%",
                                                    },
                                                    "region": {
                                                        "startLine": 7,
                                                        "startColumn": 4,
                                                    },
                                                },
                                            },
                                        ],
                                    },
                                    {
                                        "level": "error",
                                        "message": {"text": "null dereference"},
                                        "locations": [
                                            {
                                                "physicalLocation": {
                                                    "artifactLocation": {
                                                        "uri": "./src/bar.cc",
                                                        "uriBaseId": "%SRCROOT%",
                                                    },
                                                    "region": {
                                                        "startLine": 12,
                                                        "startColumn": 9,
                                                    },
                                                },
                                            },
                                        ],
                                    },
                                ],
                            },
                        ],
                    }
                ),
                encoding="utf-8",
            )

            findings = generate_dashboard.load_linter_findings(sarif_path)

        self.assertIsNotNone(findings)
        self.assertEqual(findings["errors"], 1)
        self.assertEqual(findings["warnings"], 1)
        self.assertEqual(findings["notes"], 0)
        self.assertEqual(findings["total"], 2)
        self.assertEqual(findings["findings"][0]["path"], "./src/foo.cc")
        self.assertEqual(findings["findings"][0]["name"], "")
        self.assertEqual(findings["findings"][1]["line"], 12)

    def test_main_generates_dashboard_from_clang_tidy_and_clippy_sarif(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            tmp_path = pathlib.Path(tmp_dir)
            lcov_path = tmp_path / "coverage.dat"
            clang_tidy_path = tmp_path / "clang-tidy.sarif"
            clippy_path = tmp_path / "clippy.sarif"
            codeql_path = tmp_path / "codeql.csv"
            html_path = tmp_path / "index.html"
            history_path = tmp_path / "history.json"
            summary_path = tmp_path / "summary.md"

            lcov_path.write_text(
                "\n".join(
                    [
                        "SF:src/demo.cc",
                        "DA:1,1",
                        "DA:2,0",
                        "LF:2",
                        "LH:1",
                        "FNF:1",
                        "FNH:1",
                        "BRF:2",
                        "BRH:1",
                        "end_of_record",
                    ]
                ),
                encoding="utf-8",
            )
            clang_tidy_path.write_text(
                json.dumps(
                    {
                        "$schema": "https://docs.oasis-open.org/sarif/sarif/v2.1.0/errata01/os/schemas/sarif-schema-2.1.0.json",
                        "version": "2.1.0",
                        "runs": [
                            {
                                "tool": {"driver": {"name": "ClangTidy"}},
                                "results": [
                                    {
                                        "level": "warning",
                                        "message": {"text": "use nullptr [modernize-use-nullptr]"},
                                        "locations": [
                                            {
                                                "physicalLocation": {
                                                    "artifactLocation": {"uri": "./src/demo.cc", "uriBaseId": "%SRCROOT%"},
                                                    "region": {"startLine": 4, "startColumn": 2},
                                                },
                                            },
                                        ],
                                    },
                                    {
                                        "level": "error",
                                        "message": {"text": "analyzer error [clang-analyzer-core.CallAndMessage]"},
                                        "locations": [
                                            {
                                                "physicalLocation": {
                                                    "artifactLocation": {"uri": "./src/demo.cc", "uriBaseId": "%SRCROOT%"},
                                                    "region": {"startLine": 11, "startColumn": 6},
                                                },
                                            },
                                        ],
                                    },
                                ],
                            },
                        ],
                    }
                ),
                encoding="utf-8",
            )
            clippy_path.write_text(
                json.dumps(
                    {
                        "$schema": "https://docs.oasis-open.org/sarif/sarif/v2.1.0/errata01/os/schemas/sarif-schema-2.1.0.json",
                        "version": "2.1.0",
                        "runs": [
                            {
                                "tool": {
                                    "driver": {
                                        "name": "clippy",
                                        "informationUri": "https://rust-lang.github.io/rust-clippy/",
                                        "rules": [
                                            {
                                                "id": "clippy::needless_return",
                                                "fullDescription": {
                                                    "text": "lint docs",
                                                },
                                                "helpUri": "https://rust-lang.github.io/rust-clippy/master/index.html#needless_return",
                                            },
                                        ],
                                    },
                                },
                                "results": [
                                    {
                                        "ruleId": "clippy::needless_return",
                                        "ruleIndex": 0,
                                        "level": "warning",
                                        "message": {"text": "remove needless return"},
                                        "locations": [
                                            {
                                                "physicalLocation": {
                                                    "artifactLocation": {
                                                        "uri": "score/example/src/main.rs",
                                                    },
                                                    "region": {
                                                        "startLine": 18,
                                                        "startColumn": 9,
                                                        "endLine": 18,
                                                        "endColumn": 21,
                                                    },
                                                },
                                            },
                                        ],
                                        "relatedLocations": [
                                            {
                                                "physicalLocation": {
                                                    "artifactLocation": {
                                                        "uri": "score/example/src/main.rs",
                                                    },
                                                    "region": {
                                                        "startLine": 16,
                                                        "startColumn": 9,
                                                        "endLine": 16,
                                                        "endColumn": 32,
                                                    },
                                                },
                                                "message": {
                                                    "text": "consider removing explicit return",
                                                },
                                            },
                                        ],
                                    },
                                    {
                                        "level": "note",
                                        "message": {"text": "style note"},
                                        "locations": [
                                            {
                                                "physicalLocation": {
                                                    "artifactLocation": {
                                                        "uri": "score/example/src/main.rs",
                                                    },
                                                    "region": {
                                                        "startLine": 23,
                                                        "startColumn": 5,
                                                    },
                                                },
                                            },
                                        ],
                                    },
                                ],
                            },
                        ],
                    }
                ),
                encoding="utf-8",
            )
            codeql_path.write_text(
                "severity,name,message,path,start:line\nwarning,Rule,Example finding,src/demo.cc,9\n",
                encoding="utf-8",
            )

            previous_argv = sys.argv[:]
            previous_summary = os.environ.get("GITHUB_STEP_SUMMARY")
            sys.argv = [
                "generate_dashboard.py",
                "--lcov",
                str(lcov_path),
                "--clang-tidy",
                str(clang_tidy_path),
                "--clippy",
                str(clippy_path),
                "--codeql-csv",
                str(codeql_path),
                "--html",
                str(html_path),
                "--history",
                str(history_path),
                "--github-summary",
            ]
            os.environ["GITHUB_STEP_SUMMARY"] = str(summary_path)
            try:
                exit_code = generate_dashboard.main()
            finally:
                sys.argv = previous_argv
                if previous_summary is None:
                    os.environ.pop("GITHUB_STEP_SUMMARY", None)
                else:
                    os.environ["GITHUB_STEP_SUMMARY"] = previous_summary

            self.assertEqual(exit_code, 0)
            html = html_path.read_text(encoding="utf-8")
            summary = summary_path.read_text(encoding="utf-8")
            history = json.loads(history_path.read_text(encoding="utf-8"))

        self.assertIn("Clang-Tidy", html)
        self.assertIn("Clippy", html)
        self.assertIn("Total Findings", html)
        self.assertIn("### Clippy", summary)
        self.assertIn("**2**", summary)
        self.assertEqual(history[-1]["ct_errors"], 1)
        self.assertEqual(history[-1]["ct_warnings"], 1)
        self.assertEqual(history[-1]["clippy_errors"], 0)
        self.assertEqual(history[-1]["clippy_warnings"], 1)
        self.assertEqual(history[-1]["clippy_total"], 2)


if __name__ == "__main__":
    unittest.main()



