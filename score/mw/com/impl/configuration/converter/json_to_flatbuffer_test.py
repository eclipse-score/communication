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
"""Tests for the JSON-to-FlatBuffer configuration converter."""

import json
import os
import shutil
import subprocess
import tempfile
import unittest

import json_to_flatbuffer


_CONVERTER_DIR = os.path.dirname(os.path.abspath(__file__))
_CONFIG_DIR = os.path.dirname(_CONVERTER_DIR)
_SCHEMA = os.path.join(_CONFIG_DIR, "mw_com_config.fbs")
_EXAMPLE = os.path.join(_CONFIG_DIR, "example", "mw_com_config.json")


def _flatc():
    """Locate the flatc binary provided via the FLATC_PATH runfiles env or PATH."""
    from_env = os.environ.get("FLATC_PATH")
    if from_env:
        return os.path.abspath(from_env)
    return shutil.which("flatc")


class NormalizeTest(unittest.TestCase):
    """Unit tests for the in-memory JSON normalization (no flatc involved)."""

    def test_renames_keys_and_maps_enums(self):
        config = {
            "serviceTypes": [],
            "serviceInstances": [
                {
                    "instanceSpecifier": "abc/port",
                    "serviceTypeName": "/svc",
                    "version": {"major": 1, "minor": 2},
                    "instances": [
                        {
                            "asil-level": "B",
                            "binding": "SHM",
                            "shm-size": 42,
                            "permission-checks": "file-permissions-on-empty",
                            "allowedConsumer": {"QM": [1], "B": [2]},
                        }
                    ],
                }
            ],
            "global": {
                "asil-level": "QM",
                "queue-size": {"QM-receiver": 8, "B-receiver": 5, "B-sender": 12},
                "shm-size-calc-mode": "SIMULATION",
            },
        }

        normalized = json_to_flatbuffer.normalize(config)

        instance = normalized["service_instances"][0]["instances"][0]
        self.assertEqual(instance["asil_level"], "B")
        self.assertEqual(instance["binding"], "SHM")
        self.assertEqual(instance["shm_size"], 42)
        self.assertEqual(instance["permission_checks"], "FILE_PERMISSIONS_ON_EMPTY")
        self.assertEqual(instance["allowed_consumer"], {"qm": [1], "b": [2]})
        self.assertEqual(normalized["global"]["queue_size"]["qm_receiver"], 8)
        self.assertEqual(normalized["global"]["shm_size_calc_mode"], "SIMULATION")

    def test_omits_absent_optional_keys(self):
        config = {
            "serviceTypes": [],
            "serviceInstances": [],
            "tracing": {"applicationInstanceID": "APP"},
        }
        normalized = json_to_flatbuffer.normalize(config)
        # Absent optional keys must not appear, so optional scalars stay unset.
        self.assertEqual(normalized["tracing"], {"application_instance_id": "APP"})
        self.assertNotIn("global", normalized)

    def test_unknown_key_is_rejected(self):
        with self.assertRaises(json_to_flatbuffer.ConversionError):
            json_to_flatbuffer.normalize(
                {"serviceTypes": [], "serviceInstances": [], "bogus": 1}
            )

    def test_invalid_enum_value_is_rejected(self):
        config = {
            "serviceTypes": [],
            "serviceInstances": [
                {
                    "instanceSpecifier": "p",
                    "serviceTypeName": "/svc",
                    "version": {"major": 1, "minor": 0},
                    "instances": [{"asil-level": "NOPE", "binding": "SHM"}],
                }
            ],
        }
        with self.assertRaises(json_to_flatbuffer.ConversionError):
            json_to_flatbuffer.normalize(config)


class ConvertExampleTest(unittest.TestCase):
    """End-to-end test: convert the example config and round-trip it via flatc."""

    def setUp(self):
        self.flatc = _flatc()
        if not self.flatc or not os.path.exists(self.flatc):
            self.skipTest("flatc binary not available")
        self.work_dir = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.work_dir, ignore_errors=True)

    def _roundtrip_to_json(self, bin_path, defaults=False):
        """Re-emit a FlatBuffer binary back to a JSON dict using flatc."""
        out_dir = os.path.join(self.work_dir, "rt")
        os.makedirs(out_dir, exist_ok=True)
        command = [self.flatc, "--json", "--strict-json", "--raw-binary"]
        if defaults:
            command.append("--defaults-json")
        command += ["-o", out_dir, _SCHEMA, "--", bin_path]
        subprocess.run(command, check=True)
        stem = os.path.splitext(os.path.basename(bin_path))[0]
        with open(os.path.join(out_dir, stem + ".json"), "r", encoding="utf-8") as result:
            return json.load(result)

    def test_example_converts_and_roundtrips(self):
        out_bin = os.path.join(self.work_dir, "mw_com_config.bin")
        json_to_flatbuffer.convert(_EXAMPLE, _SCHEMA, out_bin, self.flatc)

        self.assertTrue(os.path.exists(out_bin))
        self.assertGreater(os.path.getsize(out_bin), 0)

        data = self._roundtrip_to_json(out_bin)

        service_type = data["service_types"][0]
        self.assertEqual(
            service_type["service_type_name"],
            "/score/ncar/services/TirePressureService",
        )
        binding = service_type["bindings"][0]
        self.assertEqual(binding["binding"], "SHM")
        self.assertEqual(binding["service_id"], 1234)

        instance = data["service_instances"][0]["instances"][0]
        self.assertEqual(instance["asil_level"], "B")
        self.assertEqual(instance["shm_size"], 10000)
        self.assertEqual(instance["allowed_consumer"]["qm"], [42, 43])
        self.assertTrue(instance["inter_vm_support"])

        event = instance["events"][0]
        self.assertEqual(event["number_of_sample_slots"], 50)
        # enforceMaxSamples was absent in the input, so the optional stays unset.
        self.assertNotIn("enforce_max_samples", event)

        self.assertEqual(data["global"]["queue_size"]["qm_receiver"], 8)
        self.assertEqual(data["global"]["asil_level"], "B")
        # shm-size-calc-mode is SIMULATION, which equals the enum default, so
        # flatc legitimately omits it unless defaults are emitted (see below).
        self.assertNotIn("shm_size_calc_mode", data["global"])
        self.assertEqual(
            data["tracing"]["application_instance_id"], "ara_com_example"
        )

    def test_schema_defaults_applied_when_emitted(self):
        # permission-checks and shm-size-calc-mode resolve to their schema
        # defaults; flatc should surface them when defaults are emitted.
        out_bin = os.path.join(self.work_dir, "mw_com_config.bin")
        json_to_flatbuffer.convert(_EXAMPLE, _SCHEMA, out_bin, self.flatc)
        data = self._roundtrip_to_json(out_bin, defaults=True)
        instance = data["service_instances"][0]["instances"][0]
        self.assertEqual(instance["permission_checks"], "FILE_PERMISSIONS_ON_EMPTY")
        self.assertEqual(data["global"]["shm_size_calc_mode"], "SIMULATION")


if __name__ == "__main__":
    unittest.main()
