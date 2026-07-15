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
    def test_docs_flag_parser(self):
        self.assertTrue(diff_api._should_check_docs(["--check-docs"]))
        self.assertFalse(diff_api._should_check_docs([]))

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


if __name__ == "__main__":
    unittest.main()
