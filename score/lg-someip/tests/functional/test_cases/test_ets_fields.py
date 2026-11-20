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

"""Field getter, setter and notifier tests for the ETS SOME/IP service.

The tests use three fields:
  InterfaceVersion    - read-only; getter(37) + notifier(event_id=5)
  TestFieldUINT8      - read-write; getter(38) + setter(39) + notifier(event_id=6)
  TestFieldUINT8Array - read-write; getter(40) + setter(41) + notifier(event_id=7)

Wire format:
  InterfaceVersion   : 1 B (uint8 majorVersion) + 4 B (uint32 BE minorVersion)
  TestFieldUINT8     : 1 B (uint8)
  TestFieldUINT8Array: 4 B BE length prefix + N bytes

All three field notifiers are in eventgroup 5 (EVENTGROUP_UNICAST).
resetInterface (method 1, fire-and-forget) sets all fields to their initial values.
"""

from __future__ import annotations

import asyncio

import pytest
from attribute_plugin import add_test_properties
from someip_itf.ets.client import MessageType, ReturnCode
from someip_itf.ets.constants import (
    EVENT_FIELD_UINT8_ARRAY_NOTIFIER,
    EVENT_FIELD_UINT8_NOTIFIER,
    EVENT_INTERFACE_VERSION_NOTIFIER,
    FIELD_UINT8_ARRAY_INIT,
    FIELD_UINT8_INIT,
    METHOD_FIELD_UINT8_ARRAY_GET,
    METHOD_FIELD_UINT8_ARRAY_SET,
    METHOD_FIELD_UINT8_GET,
    METHOD_FIELD_UINT8_SET,
    METHOD_INTERFACE_VERSION_GET,
    METHOD_RESET_INTERFACE,
    call,
    trigger,
)
from someip_itf.ets.payloads import pack_array, unpack_array, unpack_version

pytestmark = pytest.mark.asyncio(loop_scope="class")

_NOTIFIER_SUBSCRIBE_DELAY = 1.0  # seconds to wait for the ack after subscribe
_NOTIFIER_TIMEOUT = 10.0  # seconds to wait for a notifier event
_RESET_DELAY = 0.1  # seconds to let resetInterface take effect


class TestFields:
    @add_test_properties(
        partially_verifies=["SOMEIPSRV_RPC_03"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_initial_getter_values(self, ets_client):
        """Each field getter returns its initial value after resetInterface."""
        await trigger(ets_client, METHOD_RESET_INTERFACE, b"")
        await asyncio.sleep(_RESET_DELAY)

        ver_result = await call(ets_client, METHOD_INTERFACE_VERSION_GET, b"")
        assert ver_result.message_type == MessageType.RESPONSE
        assert ver_result.return_code == ReturnCode.E_OK
        major, minor = unpack_version(ver_result.payload)
        assert (major, minor) == (1, 0), f"InterfaceVersion: expected (1, 0), got ({major}, {minor})"

        u8_result = await call(ets_client, METHOD_FIELD_UINT8_GET, b"")
        assert u8_result.message_type == MessageType.RESPONSE
        assert u8_result.return_code == ReturnCode.E_OK
        assert u8_result.payload[0] == FIELD_UINT8_INIT, (
            f"TestFieldUINT8: expected 0x{FIELD_UINT8_INIT:02x}, got 0x{u8_result.payload[0]:02x}"
        )

        arr_result = await call(ets_client, METHOD_FIELD_UINT8_ARRAY_GET, b"")
        assert arr_result.message_type == MessageType.RESPONSE
        assert arr_result.return_code == ReturnCode.E_OK
        assert unpack_array(arr_result.payload) == FIELD_UINT8_ARRAY_INIT, (
            f"TestFieldUINT8Array: expected {FIELD_UINT8_ARRAY_INIT}, got {unpack_array(arr_result.payload)}"
        )

    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_166", "SOMEIPSRV_RPC_11"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_set_get_uint8(self, ets_client):
        """The Getter returns the uint8 that the Setter wrote.

        The test calls the Setter, then the Getter. It checks the value in the two responses.
        0x42 is not FIELD_UINT8_INIT (0x55). Thus the Get returns 0x42 only if the Set is effective.
        """
        await trigger(ets_client, METHOD_RESET_INTERFACE, b"")
        await asyncio.sleep(_RESET_DELAY)

        set_result = await call(ets_client, METHOD_FIELD_UINT8_SET, bytes([0x42]))
        assert set_result.message_type == MessageType.RESPONSE
        assert set_result.return_code == ReturnCode.E_OK
        assert set_result.payload[0] == 0x42, f"Expected 0x42 after Set, got 0x{set_result.payload[0]:02x}"

        get_result = await call(ets_client, METHOD_FIELD_UINT8_GET, b"")
        assert get_result.message_type == MessageType.RESPONSE
        assert get_result.return_code == ReturnCode.E_OK
        assert get_result.payload[0] == 0x42, f"Expected 0x42 after Set, got 0x{get_result.payload[0]:02x}"

    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_167"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_set_get_array(self, ets_client):
        """The Getter returns the array that the Setter wrote.

        The test does not check the payload of the Setter response. The Getter reads the value back.
        [1, 2, 3, 4, 5] is not FIELD_UINT8_ARRAY_INIT. Thus the Get returns it only if the Set is effective.
        """
        await trigger(ets_client, METHOD_RESET_INTERFACE, b"")
        await asyncio.sleep(_RESET_DELAY)

        new_values = [1, 2, 3, 4, 5]
        set_result = await call(ets_client, METHOD_FIELD_UINT8_ARRAY_SET, pack_array(new_values))
        assert set_result.message_type == MessageType.RESPONSE
        assert set_result.return_code == ReturnCode.E_OK

        get_result = await call(ets_client, METHOD_FIELD_UINT8_ARRAY_GET, b"")
        assert get_result.message_type == MessageType.RESPONSE
        assert get_result.return_code == ReturnCode.E_OK
        assert unpack_array(get_result.payload) == new_values, (
            f"Expected {new_values} after Set, got {unpack_array(get_result.payload)}"
        )

    async def test_setter_triggers_notifier(self, ets_client, ets_field_eventgroup):
        await trigger(ets_client, METHOD_RESET_INTERFACE, b"")
        await asyncio.sleep(_RESET_DELAY)

        received: list[int] = []
        done = asyncio.Event()

        def _on_event(event_id: int, payload: bytes) -> None:
            if event_id == EVENT_FIELD_UINT8_NOTIFIER:
                received.append(payload[0])
                done.set()

        ets_client.register_callback(_on_event)
        ets_client.subscribe_eventgroup(ets_field_eventgroup, 30)
        await asyncio.sleep(_NOTIFIER_SUBSCRIBE_DELAY)
        try:
            set_result = await call(ets_client, METHOD_FIELD_UINT8_SET, bytes([0x99]))
            assert set_result.return_code == ReturnCode.E_OK
            await asyncio.wait_for(done.wait(), timeout=_NOTIFIER_TIMEOUT)
        finally:
            ets_client.unsubscribe_eventgroup(ets_field_eventgroup)
            ets_client.register_callback(None)

        assert len(received) >= 1, "No notifier event received after Set"
        assert received[-1] == 0x99, f"Expected notifier value 0x99, got 0x{received[-1]:02x}"

    async def test_getter_no_notifier(self, ets_client, ets_field_eventgroup):
        await trigger(ets_client, METHOD_RESET_INTERFACE, b"")
        await asyncio.sleep(_RESET_DELAY)

        notifier_count = [0]

        def _on_event(event_id: int, payload: bytes) -> None:  # noqa: ARG001
            if event_id == EVENT_FIELD_UINT8_NOTIFIER:
                notifier_count[0] += 1

        ets_client.register_callback(_on_event)
        ets_client.subscribe_eventgroup(ets_field_eventgroup, 30)
        await asyncio.sleep(_NOTIFIER_SUBSCRIBE_DELAY)
        # The subscription itself brings the field's initial event; only notifications after it count.
        notifier_count[0] = 0
        try:
            for _ in range(10):
                result = await call(ets_client, METHOD_FIELD_UINT8_GET, b"")
                assert result.return_code == ReturnCode.E_OK
            await asyncio.sleep(1.0)
        finally:
            ets_client.unsubscribe_eventgroup(ets_field_eventgroup)
            ets_client.register_callback(None)

        assert notifier_count[0] == 0, f"Expected zero notifier events from Get, got {notifier_count[0]}"

    # Two Setter calls at the same time.
    async def test_concurrent_setters(self, ets_client):
        await trigger(ets_client, METHOD_RESET_INTERFACE, b"")
        await asyncio.sleep(_RESET_DELAY)

        results = await asyncio.gather(
            call(ets_client, METHOD_FIELD_UINT8_SET, bytes([0x11])),
            call(ets_client, METHOD_FIELD_UINT8_SET, bytes([0x22])),
            return_exceptions=True,
        )

        for r in results:
            assert not isinstance(r, Exception), f"Set call raised: {r}"
            assert r.return_code == ReturnCode.E_OK

        assert await ets_client.is_available(), "Service unavailable after concurrent Sets"

        get_result = await call(ets_client, METHOD_FIELD_UINT8_GET, b"")
        assert get_result.return_code == ReturnCode.E_OK
        final_value = get_result.payload[0]
        assert final_value in (0x11, 0x22), f"Final value 0x{final_value:02x} is not one of the two written values"

    async def test_interface_version_no_setter(self, ets_client):
        # The generated C++ proxy has no setter. The compiler checks this.
        # This test makes sure that the getter returns the expected initial value.
        await trigger(ets_client, METHOD_RESET_INTERFACE, b"")
        await asyncio.sleep(_RESET_DELAY)

        result = await call(ets_client, METHOD_INTERFACE_VERSION_GET, b"")
        assert result.message_type == MessageType.RESPONSE
        assert result.return_code == ReturnCode.E_OK
        major, minor = unpack_version(result.payload)
        assert (major, minor) == (1, 0), f"InterfaceVersion getter: expected (1, 0), got ({major}, {minor})"

    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_146"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_reset_restores_fields(self, ets_client):
        """resetInterface sets the two writable fields back to their initial values.

        The test does not call the Getter before the Set. It does not check the value in the Setter response.
        """
        set_u8 = await call(ets_client, METHOD_FIELD_UINT8_SET, bytes([0xFF]))
        assert set_u8.return_code == ReturnCode.E_OK

        set_arr = await call(ets_client, METHOD_FIELD_UINT8_ARRAY_SET, pack_array([0xFF, 0xFF, 0xFF]))
        assert set_arr.return_code == ReturnCode.E_OK

        await trigger(ets_client, METHOD_RESET_INTERFACE, b"")
        await asyncio.sleep(_RESET_DELAY)

        u8_result = await call(ets_client, METHOD_FIELD_UINT8_GET, b"")
        assert u8_result.return_code == ReturnCode.E_OK
        assert u8_result.payload[0] == FIELD_UINT8_INIT, (
            f"After reset, TestFieldUINT8: expected 0x{FIELD_UINT8_INIT:02x}, got 0x{u8_result.payload[0]:02x}"
        )

        arr_result = await call(ets_client, METHOD_FIELD_UINT8_ARRAY_GET, b"")
        assert arr_result.return_code == ReturnCode.E_OK
        assert unpack_array(arr_result.payload) == FIELD_UINT8_ARRAY_INIT, (
            f"After reset, TestFieldUINT8Array: expected {FIELD_UINT8_ARRAY_INIT}, "
            f"got {unpack_array(arr_result.payload)}"
        )

    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_087", "SOMEIP_ETS_121"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_all_fields_notify_immediately_after_subscribe(self, ets_client, ets_field_eventgroup):
        """A Subscribe to eventgroup 5 makes the server send all 3 notifiers.

        The test does not check the SubscribeEventgroupAck or the delay after it.
        After a reset, the test subscribes and waits for each of the three notifier events.
        It does not call a Getter or a Setter.
        """
        await trigger(ets_client, METHOD_RESET_INTERFACE, b"")
        await asyncio.sleep(_RESET_DELAY)

        received_event_ids: set[int] = set()
        done = asyncio.Event()
        expected = {EVENT_INTERFACE_VERSION_NOTIFIER, EVENT_FIELD_UINT8_NOTIFIER, EVENT_FIELD_UINT8_ARRAY_NOTIFIER}

        def _on_event(event_id: int, payload: bytes) -> None:  # noqa: ARG001
            received_event_ids.add(event_id)
            if received_event_ids >= expected:
                done.set()

        ets_client.register_callback(_on_event)
        ets_client.subscribe_eventgroup(ets_field_eventgroup, 30)
        try:
            await asyncio.wait_for(done.wait(), timeout=_NOTIFIER_TIMEOUT)
        except asyncio.TimeoutError:
            pass
        finally:
            ets_client.unsubscribe_eventgroup(ets_field_eventgroup)
            ets_client.register_callback(None)

        missing = expected - received_event_ids
        assert not missing, (
            f"expected an initial notification for every field notifier "
            f"({[f'0x{e:04x}' for e in sorted(expected)]}) right after Subscribe with no Getter call; "
            f"missing {[f'0x{e:04x}' for e in sorted(missing)]}"
        )
