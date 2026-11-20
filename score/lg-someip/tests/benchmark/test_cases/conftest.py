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

"""ITF fixtures for the SOME/IP send-path and RPC benchmarks.

The someip-daemon, the app and tcpdump run on the target. someipy runs on the host.
The target is a QNX qemu or a QNX device. The ITF qemu or hardware plugin controls it over SSH.
Each run uploads the daemon, the app and the app configuration to the target.
someipy is an independent stack on the host, thus the daemon sends real notifications on the wire.
The capture runs on the target. Thus T_wire and T_app are in one clock domain.
"""

from __future__ import annotations

from pathlib import Path

import pytest
from score.itf.plugins.core import determine_target_scope
from someip_bench.rpc import RpcResult, SomeipRpcBenchmark
from someip_bench.run import BenchmarkResult, Deploy, SomeipBenchmark, Topology

# The host-side subscriber script. A someipy process next to the test runs it.
_SUBSCRIBER = Path(__file__).resolve().parent / "subscriber" / "someipy_subscriber.py"

# The host-side Echo server script. The RPC benchmark runs it next to the test.
_ECHO_SERVER = Path(__file__).resolve().parent / "server" / "someipy_echo_server.py"

# Target-side scratch directory for the pcap, the send-log, the sampler log, the process logs and the deploy.
# --target-tmp-dir can change it, because it is a property of the target, not of the benchmark.
# The QNX boot image links /tmp to /dev/shmem. That is RAM and does not support rename().
# Thus a board must set this to a real partition.
_DEFAULT_TARGET_TMP_DIR = "/tmp"


def pytest_addoption(parser):
    parser.addoption(
        "--measure",
        default="latency",
        choices=("latency", "resources"),
        help="Which tests run: latency runs every test without the resources marker, resources only the marked ones.",
    )
    parser.addoption("--benchmark-transport", default="udp", choices=("udp", "tcp"))
    parser.addoption("--num-clients", type=int, default=1)
    for name, what in (
        ("daemon", "someip-daemon binary"),
        ("app", "target app binary (bench_sender or rpc_client)"),
        ("config", "lg-someip config of the app, matching --benchmark-transport"),
    ):
        parser.addoption(f"--bench-{name}", default=None, help=f"Host path of the {what}, uploaded to the target.")
    # These options cannot be in an ITF backend configuration. Those models are pydantic with extra="forbid".
    # Also, these settings are not related to the target connection.
    # The connection and the target IP come from the ITF --hardware-config or --qemu-config.
    parser.addoption(
        "--capture-iface",
        default="eth0",
        help="Target NIC where notifications egress; the T_wire capture point.",
    )
    parser.addoption(
        "--host-subscriber-ip",
        default=None,
        help="Host IP on the target network; the someipy subscriber binds it. Required.",
    )
    parser.addoption(
        "--target-tmp-dir",
        default=_DEFAULT_TARGET_TMP_DIR,
        help="Writable scratch directory on the target for the pcap, send-log, sampler log, process logs and the "
        f"uploaded deploy. Defaults to {_DEFAULT_TARGET_TMP_DIR}; a target whose /tmp is not a real filesystem "
        "(the QNX boot image symlinks it to /dev/shmem) must point this at one.",
    )
    for name, what in (
        ("someipd", "someip-daemon"),
        ("app", "the target app (bench_sender or rpc_client)"),
        ("capture", "tcpdump"),
    ):
        parser.addoption(
            f"--{name}-cpu",
            type=_cpu,
            default=None,
            help=f"Target CPU that {what} runs on (QNX 'on -C'). Default or 'none': no pinning.",
        )


def _cpu(value: str) -> int | None:
    """A CPU number, or ``none`` for no pinning. Thus a later ``--test_arg`` can switch the pinning off."""
    return None if value == "none" else int(value)


def _measures_resources(config) -> bool:
    return config.getoption("--measure") == "resources"


def pytest_configure(config):
    config.addinivalue_line("markers", "resources: a CPU and memory test, run only with --measure=resources")


def pytest_collection_modifyitems(config, items):
    """Deselect the tests that belong to the other ``--measure`` value.

    A test with the ``resources`` marker samples CPU and memory. The sampler makes the target slower.
    Thus such a test never runs together with a latency test.
    """
    want_resources = _measures_resources(config)
    keep, dropped = [], []
    for item in items:
        (keep if (item.get_closest_marker("resources") is not None) == want_resources else dropped).append(item)
    if dropped:
        config.hook.pytest_deselected(items=dropped)
        items[:] = keep


@pytest.fixture(scope=determine_target_scope)
def transport(request) -> str:
    return request.config.getoption("--benchmark-transport")


@pytest.fixture(scope=determine_target_scope)
def num_clients(request) -> int:
    return request.config.getoption("--num-clients")


@pytest.fixture
def benchmark_run(request, target, transport, num_clients, tmp_path) -> BenchmarkResult:
    """Upload the deploy for the transport, run the full stack one time and return the artifacts.

    ``--measure`` sets if the run captures for latency or samples CPU and memory.
    The fixture uses the target for all of its duration. Thus its suites have the ``exclusive`` tag.
    UDP and TCP are different configurations of the same service instance.
    Thus each target gives its own ``--bench-config``.
    """
    topology = Topology.from_config(request.config, target)
    benchmark = SomeipBenchmark(
        target,
        topology,
        transport=transport,
        num_clients=num_clients,
        tmp_path=tmp_path,
        subscriber=_SUBSCRIBER,
        resources=_measures_resources(request.config),
    )
    benchmark.deploy(Deploy.from_config(request.config))
    return benchmark.run()


@pytest.fixture
def rpc_run(request, target, tmp_path) -> RpcResult:
    """Upload the RPC deploy, run the Echo client one time against the host server and return the call log and pcap."""
    benchmark = SomeipRpcBenchmark(
        target, Topology.from_config(request.config, target), tmp_path=tmp_path, server=_ECHO_SERVER
    )
    benchmark.deploy(Deploy.from_config(request.config))
    return benchmark.run()
