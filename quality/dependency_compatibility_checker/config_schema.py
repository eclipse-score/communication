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

"""Load and validate the dependency compatibility checker config."""

from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

import yaml

BAZEL_KEY = "bazel"
_RESERVED_DEP_KEYS = {"index", "has_patches", "flags"}
_ALLOWED_DEP_KEYS = {"module_name", "versions"}
_ALLOWED_ENTRY_KEYS = {"version", "patches", "patch_strip", "flags", "skip"}


class ConfigError(Exception):
    """Raised when the configuration is invalid."""


@dataclass
class VersionEntry:
    version: str
    patches: list = field(default_factory=list)
    patch_strip: Optional[int] = None
    flags: list = field(default_factory=list)
    skip: Optional[str] = None

    @property
    def has_patches(self) -> bool:
        return bool(self.patches)


@dataclass
class Dependency:
    key: str
    module_name: str
    versions: list  # list[VersionEntry]
    is_bazel: bool = False

    @property
    def active_versions(self) -> list:
        return [v for v in self.versions if v.skip is None]


@dataclass
class Config:
    dependencies: list  # list[Dependency]


def _parse_entry(dep_key: str, raw: dict) -> VersionEntry:
    if not isinstance(raw, dict):
        raise ConfigError(f"{dep_key}: each version must be a mapping, got {raw!r}")
    unknown = set(raw) - _ALLOWED_ENTRY_KEYS
    if unknown:
        raise ConfigError(f"{dep_key}: unknown version keys: {sorted(unknown)}")
    if "version" not in raw:
        raise ConfigError(f"{dep_key}: a version entry is missing 'version'")
    patches = list(raw.get("patches") or [])
    patch_strip = raw.get("patch_strip")
    if patch_strip is not None and not patches:
        raise ConfigError(f"{dep_key} {raw['version']}: patch_strip requires patches")
    if dep_key == BAZEL_KEY and (patches or patch_strip is not None):
        raise ConfigError("bazel: pseudo-dependency may not carry patches/patch_strip")
    return VersionEntry(
        version=str(raw["version"]),
        patches=patches,
        patch_strip=patch_strip,
        flags=list(raw.get("flags") or []),
        skip=raw.get("skip"),
    )


def _parse_dependency(key: str, raw: dict) -> Dependency:
    if not isinstance(raw, dict):
        raise ConfigError(f"{key}: dependency must be a mapping")
    if key in _RESERVED_DEP_KEYS:
        raise ConfigError(
            f"{key}: dependency name is reserved and collides with matrix metadata keys {sorted(_RESERVED_DEP_KEYS)}"
        )
    unknown = set(raw) - _ALLOWED_DEP_KEYS
    if unknown:
        raise ConfigError(f"{key}: unknown dependency keys: {sorted(unknown)}")
    raw_versions = raw.get("versions")
    if not raw_versions:
        raise ConfigError(f"{key}: must define at least one version")
    versions = [_parse_entry(key, v) for v in raw_versions]
    seen = set()
    for v in versions:
        if v.version in seen:
            raise ConfigError(f"{key}: duplicate version {v.version}")
        seen.add(v.version)
    if not any(v.skip is None for v in versions):
        raise ConfigError(f"{key}: all versions are skipped; need at least one active")
    return Dependency(
        key=key,
        module_name=str(raw.get("module_name") or key),
        versions=versions,
        is_bazel=(key == BAZEL_KEY),
    )


def load_config(path) -> Config:
    data = yaml.safe_load(Path(path).read_text(encoding="utf-8"))
    if not isinstance(data, dict) or "dependencies" not in data:
        raise ConfigError("config must be a mapping with a 'dependencies' key")
    unknown_top = set(data) - {"dependencies"}
    if unknown_top:
        raise ConfigError(f"unknown top-level keys: {sorted(unknown_top)}")
    deps_raw = data["dependencies"]
    if not isinstance(deps_raw, dict) or not deps_raw:
        raise ConfigError("'dependencies' must be a non-empty mapping")
    dependencies = [_parse_dependency(k, v) for k, v in deps_raw.items()]
    return Config(dependencies=dependencies)
