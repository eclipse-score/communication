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

"""Shared UDP receive scenarios for the routing module."""


def test_basic_udp_notification(loaded_module, record_property):
    record_property("lobster-tracing", "RoutingModule.CR_RoutingModule_UdpTransport")
    record_property("given", "a started routing module and a configured LoLa event")
    record_property("when", "a routed UDP notification is sent")
    record_property("then", "its metadata and payload are published in LoLa")
    exit_code, output = loaded_module.execute("timeout -s TERM 20 /opt/someip_routing/bin/rx_probe")
    assert exit_code == 0, output.decode(errors="replace")


def test_unknown_event_is_not_forwarded(loaded_module, record_property):
    record_property("lobster-tracing", "RoutingModule.CR_RoutingModule_ReceiveRouteMatch")
    record_property("given", "an active receive route and a subscribed LoLa event")
    record_property("when", "a notification for an unknown event is sent")
    record_property("then", "no LoLa sample is delivered for that notification")
    exit_code, output = loaded_module.execute("timeout -s TERM 20 /opt/someip_routing/bin/rx_probe wrong-event")
    assert exit_code == 0, output.decode(errors="replace")


def run_scenario(target, record_property, scenario, requirements, when, then):
    record_property("lobster-tracing", requirements)
    record_property("given", "an active UDP receive route and a subscribed LoLa event")
    record_property("when", when)
    record_property("then", then)
    exit_code, output = target.execute(f"timeout -s TERM 20 /opt/someip_routing/bin/rx_probe {scenario}")
    assert exit_code == 0, output.decode(errors="replace")


def test_wrong_sender_port_is_not_forwarded(loaded_module, record_property):
    run_scenario(
        loaded_module,
        record_property,
        "wrong-sender",
        "RoutingModule.CR_RoutingModule_ReceiveRouteMatch",
        "a notification arrives from a different UDP source port",
        "it is not delivered into LoLa",
    )


def test_wrong_local_port_is_not_forwarded(loaded_module, record_property):
    run_scenario(
        loaded_module,
        record_property,
        "wrong-port",
        "RoutingModule.CR_RoutingModule_ReceiveRouteMatch",
        "a notification arrives on another open UDP port",
        "it is not delivered to the first instance",
    )


def test_wrong_service_is_not_forwarded(loaded_module, record_property):
    run_scenario(
        loaded_module,
        record_property,
        "wrong-service",
        "RoutingModule.CR_RoutingModule_ReceiveRouteMatch",
        "a notification for another service arrives",
        "it is not delivered into LoLa",
    )


def test_non_notification_is_not_forwarded(loaded_module, record_property):
    run_scenario(
        loaded_module,
        record_property,
        "wrong-type",
        "RoutingModule.CR_RoutingModule_NotificationOnly",
        "a SOME/IP response arrives",
        "it is not delivered into LoLa",
    )


def test_malformed_notification_is_not_forwarded(loaded_module, record_property):
    run_scenario(
        loaded_module,
        record_property,
        "malformed",
        "RoutingModule.CR_RoutingModule_InvalidFraming",
        "a notification with an invalid length arrives",
        "it is not delivered into LoLa",
    )


def test_oversized_notification_is_not_forwarded(loaded_module, record_property):
    run_scenario(
        loaded_module,
        record_property,
        "oversized",
        "RoutingModule.CR_RoutingModule_OversizedPayload",
        "a notification with a 1401-byte payload arrives",
        "it is not delivered into LoLa",
    )


def test_concatenated_notifications_are_delivered_in_order(loaded_module, record_property):
    run_scenario(
        loaded_module,
        record_property,
        "concatenated",
        "RoutingModule.CR_RoutingModule_ConcatenatedMessages,RoutingModule.CR_RoutingModule_EmptyPayload,"
        "RoutingModule.CR_RoutingModule_PayloadCapacity",
        "one UDP datagram contains notifications with empty, maximum and short payloads",
        "all three notifications and their metadata are delivered in order",
    )


def test_removed_subscription_stops_delivery(loaded_module, record_property):
    run_scenario(
        loaded_module,
        record_property,
        "remove-subscription",
        "RoutingModule.CR_RoutingModule_LocalSubscriptions",
        "the local subscription is removed and a notification arrives",
        "no sample is delivered",
    )


def test_removed_remote_service_stops_delivery(loaded_module, record_property):
    run_scenario(
        loaded_module,
        record_property,
        "remove-service",
        "RoutingModule.CR_RoutingModule_RemoveRemoteService",
        "the remote service is removed and a notification arrives",
        "its LoLa offer remains stopped",
    )


def test_controller_disconnect_removes_routing_state(loaded_module, record_property):
    run_scenario(
        loaded_module,
        record_property,
        "disconnect",
        "RoutingModule.CR_RoutingModule_ControllerLoss",
        "the routing controller disconnects and a notification arrives",
        "the LoLa offer remains stopped",
    )


def test_service_removal_then_controller_reconnect(loaded_module, record_property):
    record_property(
        "lobster-tracing",
        "RoutingModule.CR_RoutingModule_RemoveRemoteService,RoutingModule.CR_RoutingModule_ControllerLoss",
    )
    record_property("given", "a routing module with a remote service and a subscribed LoLa event")
    record_property(
        "when", "the service is removed and a new controller disconnects before another controller connects"
    )
    record_property("then", "each controller can route a notification after reconnecting")
    for scenario in ("remove-service", "disconnect", "basic"):
        exit_code, output = loaded_module.execute(f"timeout -s TERM 20 /opt/someip_routing/bin/rx_probe {scenario}")
        assert exit_code == 0, output.decode(errors="replace")
