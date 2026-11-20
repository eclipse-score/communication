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

"""Shared ITF fixtures for all SOME/IP suites in this package.

The ``test_ets_*`` suites test the ETS service of the DUT.
The ``test_tc8_*`` suites test TC8 conformance against the Tc8Interface service.
The ``target`` fixture of the ITF plugin starts the container or board and sets up the network.
The fixtures in this file use that target.

At module level, this file imports only ``someip_itf.dut``, because all targets have it.
A fixture imports ``someip_itf.ets`` or ``someip_itf.tc8`` only when it needs them.
Thus an ETS-only target does not need the TC8 helper library, and a TC8-only target does not need the ETS helpers.
This conftest is the nearest one to all test modules.
Thus its ``docker_configuration`` overrides the default of the ITF docker plugin.

A test for an OPEN Alliance TC8 v3.0 case lists its ``SOMEIP_ETS_NNN`` ID in its ``add_test_properties`` decorator.
The first docstring line of the test can have a tag:

- no tag: the test does the procedure of the case and checks its pass criterion;
- ``(partial)``: the test does the same procedure, but it does not check all of the pass criterion or inputs;
- ``(variant)``: the test checks the pass criterion through a different method, eventgroup, transport or stimulus.

A tagged test gives the gap or difference on a ``Partial:`` or ``Variant:`` line in its docstring.
If the DUT does not obey a case, a strict xfail test checks the pass criterion of the case.
If the DUT does not obey a case only sometimes, the test calls ``pytest.xfail`` at the step that fails.
Thus the steps before that step still gate.
A companion test checks the actual behavior of the DUT and has no ID.
"""

from __future__ import annotations

import types

import pytest
import pytest_asyncio
from score.itf.plugins.core import determine_target_scope
from someip_itf.dut import ets_stack, provision_configs


def pytest_configure(config):
    config.addinivalue_line("markers", "tc8: a TC8 conformance case")
    config.addinivalue_line("markers", "conformance: a protocol conformance case")
    config.addinivalue_line("markers", "network: needs a non-loopback interface, and multicast on it")


def pytest_collection_modifyitems(config, items):
    """Mark each TC8 case, so that a run can select TC8 cases by tag only.

    The ETS suites also use this conftest, so the module name selects the TC8 cases.
    Each py_itf_test sandbox has only one test module.
    Without the module name check, the tag goes on the cases of each suite.
    """
    tc8_items = [item for item in items if item.module.__name__.startswith("test_tc8_")]
    for item in tc8_items:
        item.add_marker(pytest.mark.tc8)
        item.add_marker(pytest.mark.conformance)


@pytest.fixture(scope=determine_target_scope)
def docker_configuration():
    """Override for the ITF docker plugin. The lg-someip stack needs only the defaults of the plugin."""
    return {}


# ---------------------------------------------------------------------------
# ETS fixtures
# ---------------------------------------------------------------------------


@pytest.fixture(scope=determine_target_scope)
def host_ip(target) -> str:
    """The address of the DUT in the target. It is the ``unicast`` of the daemon and the SD interface."""
    return target.get_ip()


@pytest.fixture(scope=determine_target_scope)
def tester_ip(target) -> str:
    """The address that this test process on the host binds and joins on. It is the gateway of the docker bridge."""
    return target.get_gateway()


@pytest.fixture(scope=determine_target_scope)
def dut_root() -> str:
    """The directory in the image that has the daemon, the test apps and their configs."""
    from someip_itf.dut import ROOT

    return ROOT


@pytest.fixture(scope=determine_target_scope)
def dut_target(target, host_ip):
    """The target, with host_ip in its configs in place of the build-time address."""
    provision_configs(target, host_ip)
    return target


@pytest.fixture(scope=determine_target_scope)
def ets_dut(dut_target, dut_root, request):
    """The target, with the routing daemon and the ETS app in operation in it."""
    with ets_stack(dut_target, dut_root, request.config) as dut:
        yield dut


@pytest.fixture(scope="class")
def ets_eventgroup():
    """The descriptor of the ETS unicast eventgroup (id=EVENTGROUP_UNICAST)."""
    from someip_itf.ets.constants import EVENTGROUP_UNICAST

    return types.SimpleNamespace(id=EVENTGROUP_UNICAST)


@pytest.fixture(scope="class")
def ets_field_eventgroup():
    """The descriptor of the ETS field-notifier eventgroup (id=EVENTGROUP_UNICAST)."""
    from someip_itf.ets.constants import EVENTGROUP_UNICAST

    return types.SimpleNamespace(id=EVENTGROUP_UNICAST)


@pytest_asyncio.fixture(scope="class", loop_scope="class")
async def ets_client(ets_dut, tester_ip):
    """A class-scoped SOME/IP client. It is a ScapySomeipClient that finds the service with SD Find and Offer."""
    from someip_itf.ets.constants import CLIENT_ENDPOINT_PORT
    from someip_itf.ets.discovery import connect_ets_client

    client = await connect_ets_client(tester_ip, CLIENT_ENDPOINT_PORT)
    yield client
    client.close()


@pytest_asyncio.fixture(scope="class", loop_scope="class")
async def ets_client2(ets_dut, tester_ip):
    """A second class-scoped SOME/IP client, bound to CLIENT_ENDPOINT_PORT2 (60011).

    It is a different subscriber endpoint for tests that need two SD clients.
    Examples are multicast delivery and concurrent setters.
    It finds the same service as ``ets_client``, but it has its own socket and session counters.
    """
    from someip_itf.ets.constants import CLIENT_ENDPOINT_PORT2
    from someip_itf.ets.discovery import connect_ets_client

    client = await connect_ets_client(tester_ip, CLIENT_ENDPOINT_PORT2)
    yield client
    client.close()


# ---------------------------------------------------------------------------
# TC8 fixtures
#
# Use the correct address for each purpose:
#
# - ``tc8_dut_ip`` is the address where the target answers. All unicast sends go to it: FindService,
#   SubscribeEventgroup, a UDP datagram, a TCP connect.
# - ``tc8_host_ip`` and ``tc8_tester_ip`` are the address of the host on the network of the target.
#   A multicast join uses it, and a tester socket binds to it.
#
# A send to ``tc8_host_ip`` goes to the host, and then all assertions time out.
# Thus these names are different from the ETS host_ip and tester_ip above, which have a different scope.
# ---------------------------------------------------------------------------


@pytest.fixture(scope=determine_target_scope)
def tc8_network(target):
    """The addresses of the target and of the host on the docker bridge that the ITF plugin made for the target."""
    return types.SimpleNamespace(ip_address=target.get_ip(), gateway=target.get_gateway())


@pytest.fixture(scope=determine_target_scope)
def tc8_dut_ip(tc8_network) -> str:
    """The address where the target answers. All unicast sends go to it."""
    return tc8_network.ip_address


@pytest.fixture(scope=determine_target_scope)
def tc8_host_ip(tc8_network) -> str:
    """The address of the host on the network of the target. A multicast join uses this interface."""
    return tc8_network.gateway


@pytest.fixture(scope=determine_target_scope)
def tc8_tester_ip(tc8_host_ip: str) -> str:
    """The address where a tester socket binds.

    The target and the tester are different machines, so each can bind the SD port on its own address.
    """
    return tc8_host_ip


@pytest.fixture(scope="class")
def someipd_dut(tc8_network, dut_target, dut_root):
    """Start someipd and the service, and yield a handle when the target offers the service.

    The scope is per class, so a bad stack state from one suite does not go into the next suite.
    """
    from someip_itf.tc8.dut_lifecycle import start_offering_stack

    stack = start_offering_stack(dut_target, dut_root, tc8_network.gateway)
    try:
        yield stack
    finally:
        stack.stop()
