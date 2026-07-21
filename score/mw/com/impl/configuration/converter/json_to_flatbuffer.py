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
"""Convert an mw::com configuration JSON file into a FlatBuffer binary.

The runtime configuration JSON (validated against ``mw_com_config_schema.json``)
uses ``camelCase``/``kebab-case`` keys and lowercase enum strings, which do not
match the ``snake_case`` field names and enum symbols of ``mw_com_config.fbs``.

This tool performs the semantic mapping (key rename, enum-string -> enum-symbol,
dropping absent optionals) to produce a "flatc-ingestible" JSON, then delegates
serialization to ``flatc --binary`` so the resulting ``.bin`` is guaranteed to
conform to the schema. This keeps the non-trivial conversion logic here in Python
while letting flatc own the binary encoding.
"""

import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile


class ConversionError(Exception):
    """Raised when the input JSON does not match the expected configuration shape."""


# -----------------------------------------------------------------------------
# Enum string -> FlatBuffers enum symbol mappings (see mw_com_config.fbs).
# -----------------------------------------------------------------------------
_BINDING_ENUM = {"SHM": "SHM"}
_ASIL_LEVEL_ENUM = {"QM": "QM", "B": "B"}
_PERMISSION_CHECKS_ENUM = {
    "file-permissions-on-empty": "FILE_PERMISSIONS_ON_EMPTY",
    "strict": "STRICT",
}
_SHM_SIZE_CALC_MODE_ENUM = {"SIMULATION": "SIMULATION"}


# -----------------------------------------------------------------------------
# Field descriptors. Each describes how a single JSON key maps to a FlatBuffers
# field: the target field name and how to convert its value.
# -----------------------------------------------------------------------------
class _Field:
    def __init__(self, name, convert):
        self.name = name
        self._convert = convert

    def convert(self, value, path):
        return self._convert(value, path)


def _scalar(name):
    """A value copied verbatim (string / number / bool)."""
    return _Field(name, lambda value, path: value)


def _enum(name, mapping):
    """A string value translated to its FlatBuffers enum symbol."""

    def convert(value, path):
        if value not in mapping:
            raise ConversionError(
                f"{path}: invalid enum value {value!r}; expected one of "
                f"{sorted(mapping)}"
            )
        return mapping[value]

    return _Field(name, convert)


def _object(name, spec):
    """A nested object converted against ``spec``."""
    return _Field(name, lambda value, path: _convert_object(value, spec, path))


def _object_list(name, spec):
    """An array of objects, each converted against ``spec``."""

    def convert(value, path):
        if not isinstance(value, list):
            raise ConversionError(f"{path}: expected an array")
        return [_convert_object(item, spec, f"{path}[{i}]") for i, item in enumerate(value)]

    return _Field(name, convert)


def _scalar_list(name):
    """An array of scalars copied verbatim."""

    def convert(value, path):
        if not isinstance(value, list):
            raise ConversionError(f"{path}: expected an array")
        return list(value)

    return _Field(name, convert)


def _convert_object(node, spec, path):
    """Convert a JSON object ``node`` using ``spec`` (json_key -> _Field).

    Absent optional keys are simply left out, so optional FlatBuffers scalars stay
    unset. Unknown keys are a hard error: they usually indicate drift between the
    JSON schema and the FlatBuffers schema, which should fail loudly.
    """
    if not isinstance(node, dict):
        raise ConversionError(f"{path}: expected an object")
    result = {}
    for key, value in node.items():
        field = spec.get(key)
        if field is None:
            raise ConversionError(f"{path}: unknown key {key!r}")
        result[field.name] = field.convert(value, f"{path}.{key}")
    return result


# -----------------------------------------------------------------------------
# Configuration schema specs (mirror mw_com_config.fbs / mw_com_config_schema.json).
# -----------------------------------------------------------------------------
_VERSION_SPEC = {
    "major": _scalar("major"),
    "minor": _scalar("minor"),
}

# serviceTypes[]
_SERVICE_TYPE_EVENT_SPEC = {
    "eventName": _scalar("event_name"),
    "eventId": _scalar("event_id"),
}
_SERVICE_TYPE_FIELD_SPEC = {
    "fieldName": _scalar("field_name"),
    "fieldId": _scalar("field_id"),
    "Get": _scalar("get"),
    "Set": _scalar("set"),
}
_SERVICE_TYPE_METHOD_SPEC = {
    "methodName": _scalar("method_name"),
    "methodId": _scalar("method_id"),
}
_SERVICE_TYPE_BINDING_SPEC = {
    "binding": _enum("binding", _BINDING_ENUM),
    "serviceId": _scalar("service_id"),
    "events": _object_list("events", _SERVICE_TYPE_EVENT_SPEC),
    "fields": _object_list("fields", _SERVICE_TYPE_FIELD_SPEC),
    "methods": _object_list("methods", _SERVICE_TYPE_METHOD_SPEC),
}
_SERVICE_TYPE_SPEC = {
    "serviceTypeName": _scalar("service_type_name"),
    "version": _object("version", _VERSION_SPEC),
    "bindings": _object_list("bindings", _SERVICE_TYPE_BINDING_SPEC),
}

# serviceInstances[]
_UID_LIST_SPEC = {
    "QM": _scalar_list("qm"),
    "B": _scalar_list("b"),
}
_INSTANCE_EVENT_SPEC = {
    "eventName": _scalar("event_name"),
    "maxSamples": _scalar("max_samples"),
    "numberOfSampleSlots": _scalar("number_of_sample_slots"),
    "maxSubscribers": _scalar("max_subscribers"),
    "enforceMaxSamples": _scalar("enforce_max_samples"),
    "numberOfIpcTracingSlots": _scalar("number_of_ipc_tracing_slots"),
}
_INSTANCE_FIELD_SPEC = {
    "fieldName": _scalar("field_name"),
    "numberOfSampleSlots": _scalar("number_of_sample_slots"),
    "maxSubscribers": _scalar("max_subscribers"),
    "enforceMaxSamples": _scalar("enforce_max_samples"),
    "numberOfIpcTracingSlots": _scalar("number_of_ipc_tracing_slots"),
    "useGetIfAvailable": _scalar("use_get_if_available"),
    "useSetIfAvailable": _scalar("use_set_if_available"),
}
_INSTANCE_METHOD_SPEC = {
    "methodName": _scalar("method_name"),
    "queueSize": _scalar("queue_size"),
    "use": _scalar("use"),
}
_SERVICE_INSTANCE_BINDING_SPEC = {
    "instanceId": _scalar("instance_id"),
    "asil-level": _enum("asil_level", _ASIL_LEVEL_ENUM),
    "binding": _enum("binding", _BINDING_ENUM),
    "shm-size": _scalar("shm_size"),
    "control-asil-b-shm-size": _scalar("control_asil_b_shm_size"),
    "control-qm-shm-size": _scalar("control_qm_shm_size"),
    "permission-checks": _enum("permission_checks", _PERMISSION_CHECKS_ENUM),
    "allowedConsumer": _object("allowed_consumer", _UID_LIST_SPEC),
    "allowedProvider": _object("allowed_provider", _UID_LIST_SPEC),
    "events": _object_list("events", _INSTANCE_EVENT_SPEC),
    "fields": _object_list("fields", _INSTANCE_FIELD_SPEC),
    "methods": _object_list("methods", _INSTANCE_METHOD_SPEC),
    "interVmSupport": _scalar("inter_vm_support"),
    "interVmForwarded": _scalar("inter_vm_forwarded"),
}
_SERVICE_INSTANCE_SPEC = {
    "instanceSpecifier": _scalar("instance_specifier"),
    "serviceTypeName": _scalar("service_type_name"),
    "version": _object("version", _VERSION_SPEC),
    "instances": _object_list("instances", _SERVICE_INSTANCE_BINDING_SPEC),
}

# global
_QUEUE_SIZE_SPEC = {
    "QM-receiver": _scalar("qm_receiver"),
    "B-receiver": _scalar("b_receiver"),
    "B-sender": _scalar("b_sender"),
}
_GLOBAL_SPEC = {
    "asil-level": _enum("asil_level", _ASIL_LEVEL_ENUM),
    "applicationID": _scalar("application_id"),
    "queue-size": _object("queue_size", _QUEUE_SIZE_SPEC),
    "shm-size-calc-mode": _enum("shm_size_calc_mode", _SHM_SIZE_CALC_MODE_ENUM),
}

# tracing
_TRACING_SPEC = {
    "enable": _scalar("enable"),
    "applicationInstanceID": _scalar("application_instance_id"),
    "traceFilterConfigPath": _scalar("trace_filter_config_path"),
}

# root
_ROOT_SPEC = {
    "serviceTypes": _object_list("service_types", _SERVICE_TYPE_SPEC),
    "serviceInstances": _object_list("service_instances", _SERVICE_INSTANCE_SPEC),
    "global": _object("global", _GLOBAL_SPEC),
    "tracing": _object("tracing", _TRACING_SPEC),
}


def normalize(config):
    """Transform a parsed configuration dict into flatc-ingestible form."""
    return _convert_object(config, _ROOT_SPEC, "$")


def load_json(path):
    with open(path, "r", encoding="utf-8") as json_file:
        return json.load(json_file)


def run_flatc(flatc, schema, normalized_json_path, out_dir):
    """Invoke flatc to serialize ``normalized_json_path`` into ``out_dir``."""
    command = [flatc, "--binary", "-o", out_dir, schema, normalized_json_path]
    subprocess.run(command, check=True)


def convert(input_path, schema_path, output_path, flatc="flatc"):
    """Convert the config JSON at ``input_path`` into a FlatBuffer at ``output_path``."""
    normalized = normalize(load_json(input_path))

    output_dir = os.path.dirname(os.path.abspath(output_path))
    os.makedirs(output_dir, exist_ok=True)

    # flatc derives the output name from the input basename (<stem>.bin), so name
    # the temporary normalized JSON after the requested output stem.
    stem = os.path.splitext(os.path.basename(output_path))[0]
    with tempfile.TemporaryDirectory() as work_dir:
        normalized_json_path = os.path.join(work_dir, stem + ".json")
        with open(normalized_json_path, "w", encoding="utf-8") as normalized_file:
            json.dump(normalized, normalized_file)

        run_flatc(flatc, schema_path, normalized_json_path, work_dir)

        produced = os.path.join(work_dir, stem + ".bin")
        if not os.path.exists(produced):
            raise ConversionError(
                f"flatc did not produce the expected output {produced!r}"
            )
        shutil.move(produced, output_path)


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Convert an mw::com configuration JSON file into a FlatBuffer binary."
    )
    parser.add_argument("--input", required=True, help="Path to the configuration JSON file.")
    parser.add_argument("--schema", required=True, help="Path to the mw_com_config.fbs schema.")
    parser.add_argument("--output", required=True, help="Path of the FlatBuffer binary to write.")
    parser.add_argument(
        "--flatc",
        default="flatc",
        help="Path to the flatc compiler (default: %(default)s, looked up on PATH).",
    )
    args = parser.parse_args(argv)

    try:
        convert(args.input, args.schema, args.output, args.flatc)
    except ConversionError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    except subprocess.CalledProcessError as error:
        print(f"error: flatc failed with exit code {error.returncode}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
