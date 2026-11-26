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

"""Test the io-sock module's UDP receive path."""

import time

import pytest

from score.someip.test.rx_scenarios import test_basic_udp_notification as test_basic_udp_notification
from score.someip.test.rx_scenarios import (
    test_concatenated_notifications_are_delivered_in_order as test_concatenated_notifications_are_delivered_in_order,
)
from score.someip.test.rx_scenarios import (
    test_controller_disconnect_removes_routing_state as test_controller_disconnect_removes_routing_state,
)
from score.someip.test.rx_scenarios import (
    test_malformed_notification_is_not_forwarded as test_malformed_notification_is_not_forwarded,
)
from score.someip.test.rx_scenarios import (
    test_non_notification_is_not_forwarded as test_non_notification_is_not_forwarded,
)
from score.someip.test.rx_scenarios import (
    test_oversized_notification_is_not_forwarded as test_oversized_notification_is_not_forwarded,
)
from score.someip.test.rx_scenarios import (
    test_removed_remote_service_stops_delivery as test_removed_remote_service_stops_delivery,
)
from score.someip.test.rx_scenarios import (
    test_removed_subscription_stops_delivery as test_removed_subscription_stops_delivery,
)
from score.someip.test.rx_scenarios import (
    test_service_removal_then_controller_reconnect as test_service_removal_then_controller_reconnect,
)
from score.someip.test.rx_scenarios import test_unknown_event_is_not_forwarded as test_unknown_event_is_not_forwarded
from score.someip.test.rx_scenarios import (
    test_wrong_local_port_is_not_forwarded as test_wrong_local_port_is_not_forwarded,
)
from score.someip.test.rx_scenarios import (
    test_wrong_sender_port_is_not_forwarded as test_wrong_sender_port_is_not_forwarded,
)
from score.someip.test.rx_scenarios import test_wrong_service_is_not_forwarded as test_wrong_service_is_not_forwarded


@pytest.fixture(scope="session")
def loaded_module(target_init):
    target = target_init
    exit_code, output = target.execute("mount -T io-sock /opt/someip_routing/bin/mods-someip-routing.so")
    assert exit_code == 0, output.decode(errors="replace")
    return target


def test_module_loads_and_logs_to_slog(loaded_module, record_property):
    record_property("lobster-tracing", "RoutingModule.CR_RoutingModule_IoSockModule")
    record_property("given", "a QNX io-sock process and a configured routing module")
    record_property("when", "the routing module is loaded")
    record_property("then", "the module starts and its C and C++ messages reach slog2")
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        _, output = loaded_module.execute("slog2info")
        if b"Routing module started with LoLa configuration" in output and b"kernel thread started" in output:
            break
        time.sleep(0.1)
    else:
        raise AssertionError(output.decode(errors="replace"))
