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
"""Normalize a mw::com JSON configuration so ``flatc --binary`` can ingest it.

The public JSON format uses hyphenated keys (e.g. ``asil-level``) and hyphenated enum
values (e.g. ``file-permissions-on-empty``), but FlatBuffers identifiers cannot contain
hyphens, so ``mw_com_config.fbs`` spells them with underscores. This script is a thin,
*generic* preprocessor: it rewrites ``-`` -> ``_`` in object keys and in enum-valued
strings, then writes the normalized JSON back out. No field or enum name is hardcoded --
the set of enum symbols is read from the ``.fbs``-derived JSON schema, so the mapping stays
in lock-step with the single source of truth.

This module does *not* invoke ``flatc``. The build rule produces the JSON schema (via
baselibs' ``generate_json_schema``) as a separate Bazel action and passes it in via
``--schema``; keeping the normalization pure keeps it trivially testable. The enum symbols
are read shape-agnostically (any nested ``enum`` array), so either flatc's raw
``--jsonschema`` output or baselibs' post-processed rich schema works as the source.
"""

import argparse
import json
import sys


def _collect_enum_symbols(node, symbols):
    """Recursively add every string in any ``enum`` array found within ``node``."""
    if isinstance(node, dict):
        enum = node.get("enum")
        if isinstance(enum, list):
            symbols.update(value for value in enum if isinstance(value, str))
        for value in node.values():
            _collect_enum_symbols(value, symbols)
    elif isinstance(node, list):
        for item in node:
            _collect_enum_symbols(item, symbols)


def _enum_symbols_from_schema(schema_path):
    """Return the set of enum symbols (underscore form) declared in a JSON schema.

    ``schema_path`` is a ``.fbs``-derived JSON schema whose ``enum`` arrays list exactly the
    symbols of every fbs enum, so the preprocessor never hardcodes any enum name. The walk
    is shape-agnostic: it works on flatc's raw ``--jsonschema`` output (``definitions`` with
    ``enum`` arrays) as well as on baselibs' post-processed rich schema (inline ``enum`` +
    ``$defs``), since fbs enum values are preserved in underscore form in both.
    """
    with open(schema_path, encoding="utf-8") as handle:
        schema = json.load(handle)

    symbols = set()
    _collect_enum_symbols(schema, symbols)
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


def normalize(schema_path, json_path, output_path):
    """Normalize ``json_path`` against ``schema_path`` and write it to ``output_path``."""
    with open(json_path, encoding="utf-8") as handle:
        config = json.load(handle)

    normalized = _normalize(config, _enum_symbols_from_schema(schema_path))

    with open(output_path, "w", encoding="utf-8") as handle:
        json.dump(normalized, handle)


def main(argv=None):
    argv = list(sys.argv[1:] if argv is None else argv)
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--schema",
        required=True,
        help="Path to a .fbs-derived JSON schema (source of enum symbols).",
    )
    parser.add_argument("--json", required=True, help="Path to the JSON config to normalize.")
    parser.add_argument("--output", required=True, help="Where to write the normalized JSON.")
    args = parser.parse_args(argv)

    normalize(args.schema, args.json, args.output)
    return 0


if __name__ == "__main__":
    sys.exit(main())
