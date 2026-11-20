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

"""Constants and call helpers for the ETS SOME/IP test suites."""

import asyncio

# ---------------------------------------------------------------------------
# ETS service constants (from the ETS service model)
# ---------------------------------------------------------------------------

ETS_SERVICE_ID = 0x0103
ETS_INSTANCE_ID = 0x0091
ETS_MAJOR_VERSION = 2
ETS_MINOR_VERSION = 1
# The tester offers this instance. The DUT client mode finds it. TC8 calls it the "special configurable Instance ID".
ETS_REQUIRED_INSTANCE_ID = 0x00F4

METHOD_SUSPEND_INTERFACE = 2  # FIRE-AND-FORGET, UDP
METHOD_TRIGGER_EVENT_UINT8 = 3  # FIRE-AND-FORGET, UDP
METHOD_TRIGGER_EVENT_UINT8_ARRAY = 4  # FIRE-AND-FORGET, UDP
METHOD_TRIGGER_EVENT_UINT8_MULTICAST = 58  # FIRE-AND-FORGET, UDP
METHOD_ECHO_UINT8 = 8  # REQUEST/RESPONSE, UDP
METHOD_ECHO_INT8 = 14  # REQUEST/RESPONSE, UDP
METHOD_ECHO_TYPEDEF = 26  # REQUEST/RESPONSE, UDP (Build, underlying type uint8)
METHOD_ECHO_ENUM = 23  # REQUEST/RESPONSE, UDP (Enumeration1, underlying type uint8)
METHOD_CHECK_BYTE_ORDER = 31  # REQUEST/RESPONSE, UDP (u8 + u16 → u32 sum)
METHOD_ECHO_COMMON_DATATYPES = 35  # REQUEST/RESPONSE, UDP
METHOD_ECHO_STRUCT_SIMPLE = 64  # REQUEST/RESPONSE, UDP (fixed array[2] of {float, uint8})
METHOD_ECHO_UINT8_ARRAY = 9  # REQUEST/RESPONSE, UDP (dynamic, 4-byte LF)
METHOD_ECHO_UINT8_ARRAY_2DIM = 53  # REQUEST/RESPONSE, UDP (2-D dynamic)
METHOD_ECHO_STATIC_UINT8_ARRAY = 54  # REQUEST/RESPONSE, UDP (fixed size, no LF)
METHOD_ECHO_UINT8_ARRAY_MIN_SIZE = 55  # REQUEST/RESPONSE, UDP (dynamic, min-size constraint)
METHOD_ECHO_UINT8_ARRAY_8BIT_LENGTH = 62  # REQUEST/RESPONSE, UDP (name says 8-bit LF; the deployment uses 4-byte LF)
METHOD_ECHO_UINT8_ARRAY_16BIT_LENGTH = 63  # REQUEST/RESPONSE, UDP (name says 16-bit LF; the deployment uses 4-byte LF)
METHOD_ECHO_BITFIELDS = 65  # REQUEST/RESPONSE, UDP (u8, u16, u32, bits reversed)
METHOD_ECHO_FLOAT64 = 66  # REQUEST/RESPONSE, UDP
METHOD_ECHO_UINT8_RELIABLE = 72  # REQUEST/RESPONSE, TCP (same shape as echoUINT8)
METHOD_ECHO_UNION = 67  # REQUEST/RESPONSE, UDP (raw len/type/value bytes, dynamic 4-byte LF)
METHOD_ECHO_UTF16_DYNAMIC = 68  # REQUEST/RESPONSE, UDP (dynamic, 4-byte LF)
METHOD_ECHO_UTF16_FIXED = 69  # REQUEST/RESPONSE, UDP (fixed 64 bytes, no LF)
METHOD_ECHO_UTF8_DYNAMIC = 70  # REQUEST/RESPONSE, UDP (dynamic, 4-byte LF)
METHOD_ECHO_UTF8_FIXED = 71  # REQUEST/RESPONSE, UDP (fixed 64 bytes, no LF)
METHOD_ECHO_UINT8_E2E = 73  # REQUEST/RESPONSE, UDP (len, counter, dataID, crc, value: 5 u32/u8 scalars)

METHOD_RESET_INTERFACE = 1  # FIRE-AND-FORGET, UDP
METHOD_INTERFACE_VERSION_GET = 37  # REQUEST/RESPONSE, UDP (RO field getter)
METHOD_FIELD_UINT8_GET = 38  # REQUEST/RESPONSE, UDP
METHOD_FIELD_UINT8_SET = 39  # REQUEST/RESPONSE, UDP
METHOD_FIELD_UINT8_ARRAY_GET = 40  # REQUEST/RESPONSE, UDP
METHOD_FIELD_UINT8_ARRAY_SET = 41  # REQUEST/RESPONSE, UDP

METHOD_CLIENT_SERVICE_ACTIVATE = 47  # FIRE-AND-FORGET, UDP (u8 start)
METHOD_CLIENT_SERVICE_DEACTIVATE = 48  # FIRE-AND-FORGET, UDP (u8 stop)
METHOD_CLIENT_SERVICE_CALL_ECHO_UINT8_ARRAY = 49  # REQUEST/RESPONSE, UDP (4B len + bytes)
METHOD_CLIENT_SERVICE_SUBSCRIBE_EVENTGROUP = 50  # FIRE-AND-FORGET, UDP (u8 start, u8 duration)
METHOD_CLIENT_SERVICE_GET_LAST_VALUE_UNICAST = 60  # REQUEST/RESPONSE, UDP (→ u8)
METHOD_CLIENT_SERVICE_GET_LAST_VALUE_MULTICAST = 61  # REQUEST/RESPONSE, UDP (→ u8)

# On-wire notification IDs for field notifiers (0x8000 | event_id)
EVENT_INTERFACE_VERSION_NOTIFIER = 0x8005  # event_id=5 (InterfaceVersion)
EVENT_FIELD_UINT8_NOTIFIER = 0x8006  # event_id=6 (TestFieldUINT8)
EVENT_FIELD_UINT8_ARRAY_NOTIFIER = 0x8007  # event_id=7 (TestFieldUINT8Array)

FIELD_UINT8_INIT = 0x55
FIELD_UINT8_ARRAY_INIT = [0x0A, 0x0B, 0x0C]

# Array sizes from the ETS service model
STATIC_ARRAY_SIZE = 5  # fixed element count for echoStaticUINT8Array
ARRAY_MAX_SIZE = 5  # max elements for dynamic-array variants

# On-wire notification method_id = 0x8000 | event_id
EVENTGROUP_UNICAST = 5
EVENTGROUP_MULTICAST = 6  # TestEventUINT8Multicast (event_id=11)
# Multicast group of EVENTGROUP_MULTICAST. The DUT sends its events there when a 2nd client subscribes.
EVENTGROUP_MULTICAST_GROUP = ("239.192.255.250", 30514)
EVENT_UINT8 = 0x8001  # event_id=1 (TestEventUINT8)
EVENT_UINT8_ARRAY = 0x8002  # event_id=2 (TestEventUINT8Array)
EVENT_UINT8_MULTICAST = 0x800B  # event_id=11 (0x0B) | 0x8000 (TestEventUINT8Multicast, eventgroup 6)
EVENT_UINT8_E2E = 0x8004  # event ID 4 | 0x8000; both events in eventgroup 5

CLIENT_ENDPOINT_PORT = 60010
CLIENT_ENDPOINT_PORT2 = 60011  # second test-client port for two-subscriber scenarios
SERVICE_DISCOVERY_TIMEOUT = 15.0
METHOD_CALL_TIMEOUT = 10.0

# SD multicast group of the DUT deployment.
# The client must use it, not the ScapySomeipClient default of 224.224.224.245.
SD_MULTICAST_GROUP = "239.10.10.10"
SD_PORT = 30490

# Server UDP port of the ETS service instance. Tests use it when they do not use Service Discovery.
ETS_SERVER_UDP_PORT = 34501

# Server TCP port for echoUINT8RELIABLE.
ETS_SERVER_TCP_PORT = 34502

# UDP endpoints of the ETS instance of the tester (ETS_REQUIRED_INSTANCE_ID): normal, and after a reboot.
TESTER_ETS_UDP_PORT = 34999
TESTER_ETS_REBOOT_UDP_PORT = 35999
# The tester ETS sends eventgroup 6 events here. It is not EVENTGROUP_MULTICAST_GROUP, which ets_client monitors.
TESTER_ETS_MULTICAST_GROUP = ("239.192.255.251", 30515)


async def call(client, method_id: int, payload: bytes):
    return await asyncio.wait_for(
        client.call_method(method_id, payload),
        timeout=METHOD_CALL_TIMEOUT,
    )


async def trigger(client, method_id: int, payload: bytes) -> None:
    """Send a fire-and-forget method as REQUEST_NO_RETURN. The server sends no RESPONSE.

    The function returns when the datagram is sent.
    If a test needs the result of the trigger, the test must wait for its visible effect.
    """
    client.send_no_return(method_id, payload)
