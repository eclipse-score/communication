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
"""Guard test keeping ``mw_com_config_schema.json`` in sync with ``mw_com_config.fbs``.

``mw_com_config.fbs`` is the single source of truth. Regenerating the JSON schema from it
must reproduce the checked-in ``mw_com_config_schema.json`` *byte-for-byte*.

The .fbs -> post-processed-schema step (flatc ``--jsonschema`` + baselibs' schema_generator)
runs as a Bazel action; its output is provided to this test via ``--in``. ``generate_schema``
then performs the mw::com-specific hyphen restoration on it. Because the checked-in file is
itself that deterministic output, a genuine ``.fbs`` change shows up as a small, reviewable
``git diff``: just re-run ``generate_schema`` and commit.
"""

import argparse
import os
import sys
import unittest

_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, _HERE)

import generate_schema  # noqa: E402

_SCHEMA = os.path.join(_HERE, "mw_com_config_schema.json")

# Path to baselibs' post-processed schema (generate_json_schema output), passed via --in.
_POSTPROC_SCHEMA = None


class SchemaDriftTest(unittest.TestCase):
    def test_schema_matches_fbs(self):
        """The checked-in schema is byte-identical to the freshly generated one."""
        generated = generate_schema.generate(_POSTPROC_SCHEMA)
        with open(_SCHEMA, encoding="utf-8") as handle:
            committed = handle.read()
        self.assertEqual(
            committed,
            generated,
            "mw_com_config_schema.json is out of sync with mw_com_config.fbs. "
            "Re-run: bazel run "
            "//score/mw/com/impl/configuration:generate_schema  and commit the result.",
        )


if __name__ == "__main__":
    # Pull our own --in argument out of argv so unittest doesn't choke on it.
    parser = argparse.ArgumentParser()
    parser.add_argument("--in", dest="input", required=True)
    known, remaining = parser.parse_known_args()
    _POSTPROC_SCHEMA = known.input
    unittest.main(argv=[sys.argv[0]] + remaining)
