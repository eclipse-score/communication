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

import json
import tempfile
import unittest
from pathlib import Path

from quality.api_surface import diff_api


class DiffApiDocsFlagTest(unittest.TestCase):
    def compare_payloads(self, lock_payload, current_payload, check_docs=False):
        with tempfile.TemporaryDirectory() as tmpdir:
            lock_file = Path(tmpdir) / "lock.json"
            current_file = Path(tmpdir) / "current.json"
            lock_file.write_text(json.dumps(lock_payload), encoding="utf-8")
            current_file.write_text(json.dumps(current_payload), encoding="utf-8")
            return diff_api.compare(str(lock_file), str(current_file), check_docs=check_docs)

    def test_docs_flag_parser(self):
        self.assertTrue(diff_api._should_check_docs(["--check-docs"]))
        self.assertFalse(diff_api._should_check_docs([]))

    def test_compare_detects_undocumented_addition(self):
        symbol = {
            "kind": "function",
            "name": "foo",
            "qualified_name": "ns::foo",
            "signature": "foo : void ()",
        }
        self.assertEqual(
            self.compare_payloads(
                {"symbols": [], "undocumented_symbols": []},
                {"symbols": [], "undocumented_symbols": [symbol]},
            ),
            1,
        )

    def test_compare_detects_undocumented_removal(self):
        symbol = {
            "kind": "function",
            "name": "foo",
            "qualified_name": "ns::foo",
            "signature": "foo : void ()",
        }
        self.assertEqual(
            self.compare_payloads(
                {"symbols": [], "undocumented_symbols": [symbol]},
                {"symbols": [], "undocumented_symbols": []},
            ),
            1,
        )

    def test_compare_detects_undocumented_signature_change(self):
        original = {
            "kind": "function",
            "name": "foo",
            "qualified_name": "ns::foo",
            "signature": "foo : void ()",
        }
        changed = {**original, "signature": "foo : int ()"}
        self.assertEqual(
            self.compare_payloads(
                {"symbols": [], "undocumented_symbols": [original]},
                {"symbols": [], "undocumented_symbols": [changed]},
            ),
            1,
        )

    def test_compare_detects_documented_signature_change(self):
        original = {
            "kind": "function",
            "name": "foo",
            "qualified_name": "ns::foo",
            "signature": "foo : void ()",
        }
        changed = {**original, "signature": "foo : int ()"}
        self.assertEqual(
            self.compare_payloads(
                {"symbols": [original], "undocumented_symbols": []},
                {"symbols": [changed], "undocumented_symbols": []},
            ),
            1,
        )

    def test_compare_ignores_documentation_category_movement(self):
        symbol = {
            "kind": "function",
            "name": "foo",
            "qualified_name": "ns::foo",
            "signature": "foo : void ()",
        }
        self.assertEqual(
            self.compare_payloads(
                {"symbols": [symbol], "undocumented_symbols": []},
                {"symbols": [], "undocumented_symbols": [symbol]},
            ),
            0,
        )

    def test_compare_passes_identical_combined_surfaces(self):
        documented = {
            "kind": "function",
            "name": "foo",
            "qualified_name": "ns::foo",
            "signature": "foo : void ()",
        }
        undocumented = {
            "kind": "class",
            "name": "Bar",
            "qualified_name": "ns::Bar",
            "signature": "class Bar",
        }
        payload = {
            "symbols": [documented],
            "undocumented_symbols": [undocumented],
        }
        self.assertEqual(self.compare_payloads(payload, payload), 0)

    def test_compare_passes_matching_surfaces(self):
        payload = {
            "symbols": [
                {
                    "kind": "function",
                    "name": "foo",
                    "qualified_name": "ns::foo",
                    "signature": "foo : void ()",
                }
            ],
            "undocumented_symbols": [],
        }
        with tempfile.TemporaryDirectory() as tmpdir:
            lock_file = Path(tmpdir) / "lock.json"
            current_file = Path(tmpdir) / "current.json"
            lock_file.write_text(json.dumps(payload), encoding="utf-8")
            current_file.write_text(json.dumps(payload), encoding="utf-8")
            self.assertEqual(diff_api.compare(str(lock_file), str(current_file)), 0)

    def test_compare_reports_undocumented_symbols_without_location(self):
        payload = {
            "symbols": [],
            "undocumented_symbols": [
                {
                    "kind": "function",
                    "name": "foo",
                    "qualified_name": "ns::foo",
                    "signature": "foo : void ()",
                }
            ],
        }
        with tempfile.TemporaryDirectory() as tmpdir:
            lock_file = Path(tmpdir) / "lock.json"
            current_file = Path(tmpdir) / "current.json"
            lock_file.write_text(json.dumps(payload), encoding="utf-8")
            current_file.write_text(json.dumps(payload), encoding="utf-8")
            self.assertEqual(diff_api.compare(str(lock_file), str(current_file), check_docs=True), 1)

    def test_format_undocumented_symbols_omits_missing_location(self):
        report = diff_api._format_undocumented_symbols(
            {
                "undocumented_symbols": [
                    {
                        "kind": "function",
                        "name": "foo",
                        "qualified_name": "ns::foo",
                        "signature": "foo : void ()",
                    }
                ]
            }
        )
        self.assertIn("ns::foo (function)", report)
        self.assertNotIn("    at ", report)

    def test_format_undocumented_symbols_includes_location_when_present(self):
        report = diff_api._format_undocumented_symbols(
            {
                "undocumented_symbols": [
                    {
                        "kind": "function",
                        "name": "foo",
                        "qualified_name": "ns::foo",
                        "signature": "foo : void ()",
                        "file": "ns/foo.h",
                        "line": 42,
                    }
                ]
            }
        )
        self.assertIn("    at ns/foo.h:42", report)

    def test_format_undocumented_symbols_includes_file_without_line(self):
        report = diff_api._format_undocumented_symbols(
            {
                "undocumented_symbols": [
                    {
                        "kind": "function",
                        "name": "foo",
                        "qualified_name": "ns::foo",
                        "signature": "foo : void ()",
                        "file": "ns/foo.h",
                    }
                ]
            }
        )
        self.assertIn("    at ns/foo.h\n", report + "\n")
        self.assertNotIn("ns/foo.h:", report)


if __name__ == "__main__":
    unittest.main()
