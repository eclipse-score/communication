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
"""Guard tests keeping ``mw_com_config_schema.json`` in sync with ``mw_com_config.fbs``.

``mw_com_config.fbs`` is the single source of truth. Two guarantees are enforced here:

* **Drift** -- regenerating the JSON schema from the ``.fbs`` must reproduce the
  checked-in ``mw_com_config_schema.json`` *byte-for-byte*. Because the checked-in file
  is itself the generator's deterministic output, a genuine ``.fbs`` change shows up as a
  small, reviewable ``git diff``: just re-run ``generate_schema.py`` and commit.
* **Round-trip** -- an example config, preprocessed and fed through ``flatc --binary`` and
  back, must preserve every value. This proves the ``.fbs`` + preprocessor actually ingest
  a real config.

``flatc`` is located via ``$FLATC_PATH`` (set to the runfile by the bazel test), falling
back to ``flatc`` on ``PATH`` for standalone runs.
"""

import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, _HERE)

import generate_schema  # noqa: E402
from score.mw.com.impl.configuration.converter import json_to_flatbuffer  # noqa: E402

_FBS = os.path.join(_HERE, "mw_com_config.fbs")
_SCHEMA = os.path.join(_HERE, "mw_com_config_schema.json")
_EXAMPLE = os.path.join(_HERE, "example", "mw_com_config.json")


def _flatc():
    return os.environ.get("FLATC_PATH", "flatc")


class SchemaDriftTest(unittest.TestCase):
    def test_schema_matches_fbs(self):
        """The checked-in schema is byte-identical to the freshly generated one."""
        generated = generate_schema.generate(_FBS, _flatc())
        with open(_SCHEMA, encoding="utf-8") as handle:
            committed = handle.read()
        self.assertEqual(
            committed,
            generated,
            "mw_com_config_schema.json is out of sync with mw_com_config.fbs. "
            "Re-run: bazel run "
            "//score/mw/com/impl/configuration:generate_schema  and commit the result.",
        )


class RoundTripTest(unittest.TestCase):
    def test_example_config_round_trips(self):
        """Preprocess + flatc --binary + flatc --json preserves every example value.

        Reproduces the build rule's split in-process: (1) flatc --jsonschema for the enum
        symbols, (2) pure-Python normalization, (3) flatc --binary; then the reverse
        flatc --json for comparison.
        """
        flatc = _flatc()
        with open(_EXAMPLE, encoding="utf-8") as handle:
            original = json.load(handle)

        with tempfile.TemporaryDirectory() as work_dir:
            # 1. flatc --jsonschema -> schema file (the source of enum symbols).
            schema_dir = os.path.join(work_dir, "schema")
            os.makedirs(schema_dir)
            result = subprocess.run(
                [flatc, "-o", schema_dir, "--jsonschema", _FBS],
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            stem = os.path.splitext(os.path.basename(_FBS))[0]
            schema_path = os.path.join(schema_dir, stem + ".schema.json")

            # 2. normalize (pure Python, reads the schema for enum symbols).
            symbols = json_to_flatbuffer._enum_symbols_from_schema(schema_path)
            normalized = json_to_flatbuffer._normalize(original, symbols)
            normalized_path = os.path.join(work_dir, "config.json")
            with open(normalized_path, "w", encoding="utf-8") as handle:
                json.dump(normalized, handle)

            # 3. flatc --binary -> config.bin.
            bin_dir = os.path.join(work_dir, "bin")
            os.makedirs(bin_dir)
            result = subprocess.run(
                [flatc, "-o", bin_dir, "--binary", _FBS, normalized_path],
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            binary = os.path.join(bin_dir, "config.bin")

            # 4. reverse: flatc --json. --defaults-json so default-valued fields (which
            # flatc elides from the binary) reappear, giving a complete tree to compare
            # against the normalized input.
            out_dir = os.path.join(work_dir, "out")
            os.makedirs(out_dir)
            result = subprocess.run(
                [
                    flatc, "-o", out_dir, "--json", "--strict-json",
                    "--defaults-json", "--raw-binary", _FBS, "--", binary,
                ],
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            with open(os.path.join(out_dir, "config.json"), encoding="utf-8") as handle:
                round_tripped = json.load(handle)

        self._assert_subset(normalized, round_tripped, "")

    def _assert_subset(self, expected, actual, path):
        """Every value in ``expected`` must be present and equal in ``actual``."""
        if isinstance(expected, dict):
            for key, value in expected.items():
                self.assertIn(key, actual, "missing key at %s/%s" % (path, key))
                self._assert_subset(value, actual[key], "%s/%s" % (path, key))
        elif isinstance(expected, list):
            self.assertEqual(len(expected), len(actual), "list length at %s" % path)
            for index, (exp, act) in enumerate(zip(expected, actual)):
                self._assert_subset(exp, act, "%s[%d]" % (path, index))
        else:
            self.assertEqual(expected, actual, "value at %s" % path)


if __name__ == "__main__":
    # Skip gracefully when flatc is unavailable (e.g. bazel didn't provide the runfile).
    if not shutil.which(_flatc()) and not os.path.exists(_flatc()):
        print("flatc not available (FLATC_PATH unset); skipping", file=sys.stderr)
        sys.exit(0)
    unittest.main()
