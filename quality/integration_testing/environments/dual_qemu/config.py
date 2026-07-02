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
"""Configuration model for the dual-QEMU (ivshmem) plugin.

Each VM is described by the ``QemuConfigModel``, so a dual config is just two
ordinary QEMU configs plus a shared ``ivshmem`` block.
"""
import json
import logging

from pydantic import BaseModel, ConfigDict, Field, ValidationError

from score.itf.plugins.qemu.config import QemuConfigModel

logger = logging.getLogger(__name__)

_SIZE_UNITS = {"K": 1024, "M": 1024**2, "G": 1024**3, "T": 1024**4, "P": 1024**5}


def parse_size(size: str) -> int:
    """Convert a QEMU-style size string such as ``"4M"`` into a byte count."""
    return int(size[:-1]) * _SIZE_UNITS[size[-1]]


class IvshmemConfig(BaseModel):
    model_config = ConfigDict(extra="forbid")

    size: str = Field(default="4M", pattern=r"^[0-9]+[KMGTP]$")
    # Optional explicit host backing file. When empty, a temporary file is created
    # by the ``ivshmem_backend`` fixture and removed on session teardown.
    mem_path: str = ""


class InterVmNetwork(BaseModel):
    model_config = ConfigDict(extra="forbid")

    # Optional point-to-point socket NIC between the two VMs for a socket-based control
    # plane. Off by default
    enabled: bool = False
    host_port: int = Field(default=12345, ge=1, le=65535)


class DualQemuConfigModel(BaseModel):
    model_config = ConfigDict(extra="forbid")

    ivshmem: IvshmemConfig = Field(default_factory=IvshmemConfig)
    intervm_network: InterVmNetwork = Field(default_factory=InterVmNetwork)
    vms: list[QemuConfigModel] = Field(min_length=2, max_length=2)


def load_configuration(config_file: str) -> DualQemuConfigModel:
    logger.info(f"Loading dual-QEMU configuration from {config_file}")
    with open(config_file, "r", encoding="utf-8") as handle:
        raw = json.load(handle)
    try:
        return DualQemuConfigModel(**raw)
    except ValidationError as error:
        logger.error(f"Invalid dual-QEMU configuration: {error}")
        raise
