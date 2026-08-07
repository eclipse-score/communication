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
"""Convert a mw::com JSON configuration into its FlatBuffer binary via ``flatc``.

The public JSON format uses hyphenated keys (e.g. ``asil-level``) and hyphenated enum
values (e.g. ``file-permissions-on-empty``), but FlatBuffers identifiers cannot contain
hyphens, so ``mw_com_config.fbs`` spells them with underscores. This script is a thin,
*generic* preprocessor: it rewrites ``-`` -> ``_`` in object keys and in enum-valued
strings, then hands the result to ``flatc --binary``. No field or enum name is hardcoded
-- the set of enum symbols is derived from the ``.fbs`` itself, so the mapping stays in
lock-step with the single source of truth.
"""

import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile


class ConversionError(RuntimeError):
    """Raised when conversion fails."""


def _enum_symbols(flatc, fbs_path):
    """Return the set of enum symbols (underscore form) declared in the ``.fbs``.

    Derived from ``flatc --jsonschema`` output, whose ``enum`` arrays list exactly the
    symbols of every fbs enum -- so the preprocessor never hardcodes any enum name.
    """
    with tempfile.TemporaryDirectory() as out_dir:
        result = subprocess.run(
            [flatc, "-o", out_dir, "--jsonschema", fbs_path],
            capture_output=True,
            text=True,
            check=False,
        )
        if result.returncode != 0:
            raise ConversionError(
                "flatc --jsonschema failed:\n%s\n%s" % (result.stdout, result.stderr)
            )
        stem = os.path.splitext(os.path.basename(fbs_path))[0]
        with open(os.path.join(out_dir, stem + ".schema.json"), encoding="utf-8") as handle:
            schema = json.load(handle)

    symbols = set()
    for definition in schema.get("definitions", {}).values():
        for value in definition.get("enum", []):
            symbols.add(value)
        for field in definition.get("properties", {}).values():
            for value in field.get("enum", []):
                symbols.add(value)
    return symbols


def _normalize(node, enum_symbols):
    """Recursively rewrite ``-`` -> ``_`` in object keys and enum-valued strings.

    Returns the normalized node. A string value is converted only if its underscore
    form is a known enum symbol, so arbitrary strings (paths, names) are left untouched.
    """
    if isinstance(node, dict):
        return {
            key.replace("-", "_"): _normalize(value, enum_symbols)
            for key, value in node.items()
        }
    if isinstance(node, list):
        return [_normalize(item, enum_symbols) for item in node]
    if isinstance(node, str):
        candidate = node.replace("-", "_")
        if candidate in enum_symbols:
            return candidate
        return node
    return node


def _run_flatc_binary(flatc, fbs_path, json_path, out_dir):
    result = subprocess.run(
        [flatc, "-o", out_dir, "--binary", fbs_path, json_path],
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode != 0:
        raise ConversionError(
            "flatc --binary failed:\n%s\n%s" % (result.stdout, result.stderr)
        )
    stem = os.path.splitext(os.path.basename(json_path))[0]
    return os.path.join(out_dir, stem + ".bin")


def convert(fbs_path, json_path, output_path, flatc="flatc"):
    """Convert ``json_path`` to a FlatBuffer binary at ``output_path``."""
    with open(json_path, encoding="utf-8") as handle:
        config = json.load(handle)

    normalized = _normalize(config, _enum_symbols(flatc, fbs_path))

    with tempfile.TemporaryDirectory() as work_dir:
        # flatc names the output after the input stem; keep the original stem so the
        # produced ``.bin`` matches the config's name.
        source = os.path.join(work_dir, os.path.basename(json_path))
        with open(source, "w", encoding="utf-8") as handle:
            json.dump(normalized, handle)
        produced = _run_flatc_binary(flatc, fbs_path, source, work_dir)
        os.makedirs(os.path.dirname(os.path.abspath(output_path)), exist_ok=True)
        shutil.move(produced, output_path)


def main(argv=None):
    argv = list(sys.argv[1:] if argv is None else argv)
    here = os.path.dirname(os.path.abspath(__file__))
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--fbs",
        default=os.path.join(here, os.pardir, "mw_com_config.fbs"),
        help="Path to the FlatBuffers schema (default: ../mw_com_config.fbs).",
    )
    parser.add_argument("--json", required=True, help="Path to the JSON config to convert.")
    parser.add_argument("--output", required=True, help="Where to write the FlatBuffer binary.")
    parser.add_argument(
        "--flatc",
        default=os.environ.get("FLATC_PATH", "flatc"),
        help="Path to the flatc binary (default: $FLATC_PATH or 'flatc').",
    )
    args = parser.parse_args(argv)

    convert(args.fbs, args.json, args.output, args.flatc)
    return 0


if __name__ == "__main__":
    sys.exit(main())
