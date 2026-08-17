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
"""Generate ``mw_com_config_schema.json`` from the FlatBuffers single source of truth.

The FlatBuffers schema (``mw_com_config.fbs``) is the single source of truth for the
mw::com runtime configuration. The rich JSON schema is regenerated from it so that the
schema never drifts from the FlatBuffers definition.

The heavy lifting -- running ``flatc --jsonschema`` and post-processing its output into a
rich draft-2020-12 schema (splitting ``@title`` / ``@default`` / ``@min`` / ``@max`` /
``@required`` tokens out of descriptions, lifting ``@shared`` tables into ``$defs``,
etc.) -- is done by baselibs' ``schema_generator`` in a Bazel action. Its output is the
``mw_com_config.postproc.schema.json`` passed in via ``--in``.

This tool performs the **only** step that is specific to mw::com's public config format:
restoring the fbs ``-`` -> ``_`` mapping. FlatBuffers identifiers cannot contain ``-``, so
the .fbs spells hyphenated public names (e.g. ``asil-level``, ``file-permissions-on-empty``)
with underscores; here they are turned back into hyphens. Restoration is targeted -- it
touches object property keys, ``required`` entries, ``enum`` values and enum-valued
``default`` strings only, never ``$defs`` keys, ``$ref`` targets or free-form strings such
as file paths.

baselibs already fixed key ordering and ``indent=4``, so the
checked-in schema is simply this tool's committed output and a drift test can compare it
byte-for-byte.
"""

import argparse
import json
import os
import sys


def _restore_hyphens(name):
    """Reverse the fbs ``-`` -> ``_`` mapping (fbs identifiers cannot contain ``-``)."""
    return name.replace("_", "-")


def _restore_node(node):
    """Recursively restore hyphens in fbs-derived identifiers within ``node``.

    Restores ``_`` -> ``-`` in:
      * object property keys (under ``properties``);
      * ``required`` entries;
      * ``enum`` values;
      * a ``default`` string, only when a sibling (restored) ``enum`` contains it.

    Leaves everything else untouched: ``$defs`` keys, ``$ref`` targets and arbitrary
    string values (e.g. file paths) keep their underscores.
    """
    if isinstance(node, list):
        return [_restore_node(item) for item in node]
    if not isinstance(node, dict):
        return node

    out = {}
    for key, value in node.items():
        if key == "properties" and isinstance(value, dict):
            out[key] = {
                _restore_hyphens(prop): _restore_node(prop_node)
                for prop, prop_node in value.items()
            }
        elif key == "required" and isinstance(value, list):
            out[key] = [_restore_hyphens(entry) for entry in value]
        elif key == "enum" and isinstance(value, list):
            out[key] = [_restore_hyphens(entry) for entry in value]
        else:
            out[key] = _restore_node(value)

    # A default that names an enum symbol must be hyphen-restored to match its enum.
    default = out.get("default")
    enum = out.get("enum")
    if isinstance(default, str) and isinstance(enum, list):
        restored = _restore_hyphens(default)
        if restored in enum:
            out["default"] = restored
    return out


def generate(postproc_schema_path):
    """Return the final schema (as a string) by hyphen-restoring the post-processed schema.

    ``postproc_schema_path`` is baselibs' ``generate_json_schema`` output (a rich
    draft-2020-12 schema whose fbs-derived identifiers still use underscores).
    """
    with open(postproc_schema_path, "r", encoding="utf-8") as handle:
        schema = json.load(handle)

    restored = _restore_node(schema)
    return json.dumps(restored, indent=4, ensure_ascii=False) + "\n"


# Workspace-relative location of the schema, used to write back to the source tree when
# invoked via ``bazel run`` (which sets ``BUILD_WORKSPACE_DIRECTORY``).
_SCHEMA_RELPATH = "score/mw/com/impl/configuration/mw_com_config_schema.json"


def build_arg_parser():
    """Argument parser for the CLI entry point (schema_drift_test.py parses --in separately
    before handing remaining args to unittest)."""
    here = os.path.dirname(os.path.abspath(__file__))
    workspace = os.environ.get("BUILD_WORKSPACE_DIRECTORY")
    default_output = (
        os.path.join(workspace, _SCHEMA_RELPATH)
        if workspace
        else os.path.join(here, "mw_com_config_schema.json")
    )
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--in",
        dest="input",
        required=True,
        help="Path to baselibs' post-processed schema (generate_json_schema output).",
    )
    parser.add_argument(
        "--output",
        default=default_output,
        help="Where to write the schema, or '-' for stdout "
        "(default: the checked-in mw_com_config_schema.json).",
    )
    return parser


def main(argv=None):
    argv = list(sys.argv[1:] if argv is None else argv)
    args = build_arg_parser().parse_args(argv)

    schema = generate(args.input)
    if args.output == "-":
        sys.stdout.write(schema)
    else:
        with open(args.output, "w", encoding="utf-8") as handle:
            handle.write(schema)
    return 0


if __name__ == "__main__":
    sys.exit(main())
