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

"""Exercise the Linux routing module with real UDP and LoLa."""

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


@pytest.fixture
def loaded_module(target):
    with target.wrap_exec("/opt/someip_routing/bin/linux_routing_module", enforce_clean_shutdown=True):
        yield target


def test_linux_routing_process_starts(target):
    with target.wrap_exec("/opt/someip_routing/bin/linux_routing_module", enforce_clean_shutdown=True) as process:
        deadline = time.monotonic() + 10
        socket_output = b""
        while time.monotonic() < deadline and process.is_running():
            exit_code, socket_output = target.execute("cat /proc/net/unix")
            if exit_code == 0 and b"someip_routing_module" in socket_output:
                return
            time.sleep(0.1)
        raise AssertionError(f"{process.get_output()}\nSocket table: {socket_output!r}")
