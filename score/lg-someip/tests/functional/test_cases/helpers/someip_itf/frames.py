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

"""SOME/IP frames packed by hand. A raw socket uses them to put any value into each header field."""

from __future__ import annotations

import struct
from typing import Final

from someip_itf.codec import MessageType

HEADER: Final = struct.Struct(">HHIHHBBBB")
"""Header fields: service, method, length, client, session, protocol version, interface version, message type, return code."""


def someip_frame(
    service_id: int,
    method_id: int,
    payload: bytes,
    *,
    client_id: int = 0x0001,
    session_id: int = 0x0001,
    proto_ver: int = 0x01,
    iface_ver: int = 0x01,
    msg_type: int = int(MessageType.REQUEST),
    retcode: int = 0x00,
) -> bytes:
    """Return a frame. Its length field covers the last 8 header bytes plus *payload*. The other fields do not change it."""
    header = HEADER.pack(
        service_id, method_id, 8 + len(payload), client_id, session_id, proto_ver, iface_ver, msg_type, retcode
    )
    return header + payload
