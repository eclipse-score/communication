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
"""Generate ``mw_com_config_schema.json`` from ``mw_com_config.fbs``.

The FlatBuffers schema (``mw_com_config.fbs``) is the single source of truth for the
mw::com runtime configuration. This tool regenerates the rich JSON schema from it so
that the schema never drifts from the FlatBuffers definition.

Pipeline:
  1. Run ``flatc --jsonschema`` on the .fbs. flatc emits structure, ``description`` (from
     ``///`` doc comments), string ``enum`` (from fbs enums), ``deprecated`` and type-based
     integer ranges -- but NOT ``title`` / ``default`` / custom ``minimum`` / ``maximum``,
     and it uses draft 2019-09 with ``definitions`` + ``$ref``.
  2. Post-process flatc's output into the rich draft-2020-12 schema:
       * split ``@title:`` / ``@default:`` / ``@min:`` / ``@max:`` / ``@required`` token lines
         out of each ``description`` into proper JSON-schema attributes;
       * strip flatc's type-based ``minimum`` / ``maximum``, re-adding only from ``@min`` / ``@max``;
       * inline every ``$ref`` except the shared ``ServiceVersion`` (kept as ``$defs/serviceVersion``);
       * restore ``_`` -> ``-`` in object keys and enum values (fbs identifiers can't contain ``-``).

The result is deterministic (fixed key ordering, ``json.dumps(indent=4)``), so the checked-in
schema is simply this tool's committed output and a drift test can compare byte-for-byte.
"""

import argparse
import json
import os
import re
import subprocess
import sys
import tempfile

# Table whose def is emitted as ``$defs/serviceVersion`` and referenced (not inlined),
# because it is shared by both service types and service instances.
_SHARED_DEF_SUFFIX = "_ServiceVersion"
_SHARED_DEF_NAME = "serviceVersion"

_TOKEN_RE = re.compile(r"^@(title|default|min|max|required)\b\s*:?\s*(.*)$")

_DRAFT_2020_12 = "https://json-schema.org/draft/2020-12/schema"


class GenerationError(RuntimeError):
    """Raised when schema generation fails."""


def _restore_hyphens(name):
    """Reverse the fbs ``-`` -> ``_`` mapping (fbs identifiers cannot contain ``-``)."""
    return name.replace("_", "-")


def _parse_default(raw):
    """Parse an ``@default:`` token value into its JSON type (bool / int / string)."""
    if raw == "true":
        return True
    if raw == "false":
        return False
    try:
        return int(raw)
    except ValueError:
        return raw


def _parse_description(text):
    """Split a flatc ``description`` string into (metadata, clean_description).

    ``@token`` lines are extracted into a dict; the remaining lines form the human-readable
    description (joined with ``\\n``, preserving the original multi-line layout).
    """
    meta = {"title": None, "default": None, "min": None, "max": None, "required": False}
    desc_lines = []
    for line in text.split("\n"):
        match = _TOKEN_RE.match(line)
        if match is None:
            desc_lines.append(line)
            continue
        key, value = match.group(1), match.group(2).strip()
        if key == "required":
            meta["required"] = True
        elif key in ("min", "max"):
            meta[key] = int(value)
        elif key == "default":
            meta["default"] = _parse_default(value)
        else:  # title
            meta["title"] = value
    return meta, "\n".join(desc_lines)


class _Enricher:
    def __init__(self, definitions):
        self._defs = definitions

    def _ref_name(self, node):
        return node["$ref"].split("/")[-1]

    def _is_enum(self, def_name):
        return "enum" in self._defs[def_name]

    def _enum_values(self, def_name):
        return [_restore_hyphens(v) for v in self._defs[def_name]["enum"]]

    def build_field(self, node):
        """Build an enriched schema node for a property. Returns (schema_node, required)."""
        if "$ref" in node:
            ref = self._ref_name(node)
            if ref.endswith(_SHARED_DEF_SUFFIX):
                meta, _ = _parse_description(node.get("description", ""))
                return {"$ref": "#/$defs/%s" % _SHARED_DEF_NAME}, meta["required"]
            if self._is_enum(ref):
                meta, desc = _parse_description(node.get("description", ""))
                out = {"type": "string"}
                if meta["title"] is not None:
                    out["title"] = meta["title"]
                if desc:
                    out["description"] = desc
                out["enum"] = self._enum_values(ref)
                if meta["default"] is not None:
                    out["default"] = meta["default"]
                return out, meta["required"]
            # Non-shared table reference -> inline it. Such fields are authored bare
            # (metadata lives on the referenced table), so they are never required here.
            return self.build_object(self._defs[ref]), False

        node_type = node.get("type")
        if node_type == "array":
            meta, desc = _parse_description(node.get("description", ""))
            out = {"type": "array"}
            if meta["title"] is not None:
                out["title"] = meta["title"]
            if desc:
                out["description"] = desc
            out["items"] = self._build_items(node["items"])
            return out, meta["required"]

        # Scalar / string / bool leaf.
        meta, desc = _parse_description(node.get("description", ""))
        out = {"type": node_type}
        if meta["title"] is not None:
            out["title"] = meta["title"]
        if desc:
            out["description"] = desc
        if "enum" in node:  # inline enum defined directly on the node (not via $ref)
            out["enum"] = [_restore_hyphens(v) for v in node["enum"]]
        if meta["default"] is not None:
            out["default"] = meta["default"]
        if meta["min"] is not None:
            out["minimum"] = meta["min"]
        if meta["max"] is not None:
            out["maximum"] = meta["max"]
        if node.get("deprecated"):
            out["deprecated"] = True
        return out, meta["required"]

    def _build_items(self, items):
        if "$ref" in items:
            ref = self._ref_name(items)
            if ref.endswith(_SHARED_DEF_SUFFIX):
                return {"$ref": "#/$defs/%s" % _SHARED_DEF_NAME}
            if self._is_enum(ref):
                return {"type": "string", "enum": self._enum_values(ref)}
            return self.build_object(self._defs[ref])
        # Scalar vector items: drop flatc's type-based min/max (schema has none for these).
        return {"type": items["type"]}

    def build_object(self, def_node):
        meta, desc = _parse_description(def_node.get("description", ""))
        properties = {}
        required = []
        for field_name, field_node in def_node.get("properties", {}).items():
            built, is_required = self.build_field(field_node)
            key = _restore_hyphens(field_name)
            properties[key] = built
            if is_required:
                required.append(key)
        out = {"type": "object"}
        if meta["title"] is not None:
            out["title"] = meta["title"]
        if desc:
            out["description"] = desc
        if required:
            out["required"] = required
        out["additionalProperties"] = False
        out["properties"] = properties
        return out


def _find_shared_def(definitions):
    matches = [name for name in definitions if name.endswith(_SHARED_DEF_SUFFIX)]
    if len(matches) != 1:
        raise GenerationError(
            "expected exactly one %s table, found: %s" % (_SHARED_DEF_SUFFIX, matches)
        )
    return matches[0]


def _run_flatc_jsonschema(flatc, fbs_path):
    with tempfile.TemporaryDirectory() as out_dir:
        result = subprocess.run(
            [flatc, "-o", out_dir, "--jsonschema", fbs_path],
            capture_output=True,
            text=True,
            check=False,
        )
        if result.returncode != 0:
            raise GenerationError(
                "flatc --jsonschema failed:\n%s\n%s" % (result.stdout, result.stderr)
            )
        stem = os.path.splitext(os.path.basename(fbs_path))[0]
        schema_file = os.path.join(out_dir, stem + ".schema.json")
        with open(schema_file, "r", encoding="utf-8") as handle:
            return json.load(handle)


def generate(fbs_path, flatc="flatc"):
    """Return the rich JSON schema (as a string) generated from ``fbs_path``."""
    raw = _run_flatc_jsonschema(flatc, fbs_path)
    definitions = raw["definitions"]
    enricher = _Enricher(definitions)

    root_name = raw["$ref"].split("/")[-1]
    root_obj = enricher.build_object(definitions[root_name])

    shared_name = _find_shared_def(definitions)
    shared_obj = enricher.build_object(definitions[shared_name])

    schema = {"$schema": _DRAFT_2020_12}
    if "title" in root_obj:
        schema["title"] = root_obj["title"]
    if "description" in root_obj:
        schema["description"] = root_obj["description"]
    schema["type"] = "object"
    if "required" in root_obj:
        schema["required"] = root_obj["required"]
    schema["additionalProperties"] = root_obj["additionalProperties"]
    schema["properties"] = root_obj["properties"]
    schema["$defs"] = {_SHARED_DEF_NAME: shared_obj}

    return json.dumps(schema, indent=4, ensure_ascii=False) + "\n"


# Workspace-relative location of the schema, used to write back to the source tree when
# invoked via ``bazel run`` (which sets ``BUILD_WORKSPACE_DIRECTORY``).
_SCHEMA_RELPATH = "score/mw/com/impl/configuration/mw_com_config_schema.json"


def main(argv=None):
    argv = list(sys.argv[1:] if argv is None else argv)
    here = os.path.dirname(os.path.abspath(__file__))
    workspace = os.environ.get("BUILD_WORKSPACE_DIRECTORY")
    default_output = (
        os.path.join(workspace, _SCHEMA_RELPATH)
        if workspace
        else os.path.join(here, "mw_com_config_schema.json")
    )
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--fbs",
        default=os.path.join(here, "mw_com_config.fbs"),
        help="Path to the FlatBuffers schema (default: sibling mw_com_config.fbs).",
    )
    parser.add_argument(
        "--flatc",
        default=os.environ.get("FLATC_PATH", "flatc"),
        help="Path to the flatc binary (default: $FLATC_PATH or 'flatc').",
    )
    parser.add_argument(
        "--output",
        default=default_output,
        help="Where to write the schema, or '-' for stdout "
        "(default: the checked-in mw_com_config_schema.json).",
    )
    args = parser.parse_args(argv)

    schema = generate(args.fbs, args.flatc)
    if args.output == "-":
        sys.stdout.write(schema)
    else:
        with open(args.output, "w", encoding="utf-8") as handle:
            handle.write(schema)
    return 0


if __name__ == "__main__":
    sys.exit(main())
