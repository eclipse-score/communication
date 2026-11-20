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

"""SOME/IP send-path and RPC benchmarks of the lg-someip stack, on a QNX qemu or a QNX board.

Every target is `manual`: a run owns its target, needs a host that can exchange SOME/IP with it, and for the UDP
buckets above one datagram the stack built with SOME/IP-TP. lg-someip's QNX sources do not pass this repository's
warnings-as-errors, hence the per_file_copt.

qemu (x86_64), on the host tap device that `bazel run :setup_tap` creates once per boot:

  bazel test --config=qnx_x86_64 --//score/lg-someip:enable_someip_tp=True \
      '--per_file_copt=score/lg-someip/.*@-w' --test_output=streamed \
      //score/lg-someip/tests/benchmark/test_cases:benchmark_udp_qemu

board (aarch64), through ITF's hardware plugin:

  bazel test --config=qnx_arm64 --//score/lg-someip:enable_someip_tp=True \
      '--per_file_copt=score/lg-someip/.*@-w' --test_output=streamed \
      //score/lg-someip/tests/benchmark/test_cases:benchmark_udp \
      --test_arg=--hardware-config=/abs/path/device.json --test_arg=--capture-iface=<board nic> \
      --test_arg=--host-subscriber-ip=<host ip on the board's subnet> --test_arg=--target-tmp-dir=<writable dir>
"""

load("@rules_shell//shell:sh_binary.bzl", "sh_binary")
load("@score_communication_pip//:requirements.bzl", "requirement")
load("@score_itf//:defs.bzl", "py_itf_test")
load("@score_rules_imagefs//rules/qnx:ifs.bzl", "qnx_ifs")

# Host-side setup of the qemu targets' tap device, so both stay on the host platform: run them without --config.
[
    sh_binary(
        name = NAME,
        srcs = ["scripts/{}.sh".format(NAME)],
        target_compatible_with = ["@platforms//os:linux"],
    )
    for NAME in [
        "setup_tap",
        "teardown_tap",
    ]
]

_BENCH = "//score/lg-someip/tests/benchmark"

_DAEMON = "//score/lg-someip/daemon:someip-daemon"

_QNX = ["@platforms//os:qnx"]

# The stock integration-test image: the run uploads its binaries, so the image carries none of them.
qnx_ifs(
    name = "_qemu_ifs",
    srcs = ["//quality/integration_testing/environments/qnx8_qemu:qnx_config"],
    out = "qemu_ifs",
    build_file = "//quality/integration_testing/environments/qnx8_qemu:init_build",
    target_compatible_with = _QNX + ["@platforms//cpu:x86_64"],
)

_COMMON_ARGS = [
    "-vvv",
    "-s",
    "--log-cli-level=INFO",
    # One target per bazel test invocation, rather than one per test case.
    "--keep-target",
    "--bench-daemon=$(location {})".format(_DAEMON),
]

# The qemu sits on the host's tap0: the guest is 10.0.2.15 on vtnet0, the host 10.0.2.2.
# Its /tmp is RAM-backed, which is where the pcap goes.
_QEMU_ARGS = [
    "--qemu-config=$(location config/qemu_config.json)",
    "--qemu-image=$(location :_qemu_ifs)",
    "--capture-iface=vtnet0",
    "--host-subscriber-ip=10.0.2.2",
    "--target-tmp-dir=/tmp",
]

# Placeholders for an unknown board; override each with --test_arg.
_HARDWARE_ARGS = [
    "--hardware-config=$(location config/hardware_config.example.json)",
    "--capture-iface=eth0",
    "--host-subscriber-ip=192.168.2.100",
    # A QNX boot image symlinks /tmp to /dev/shmem, which is RAM-backed, so the pcap goes to a real partition.
    "--target-tmp-dir=/data/tmp",
    # Pin the daemon and the app to two different cores, not core 0. Override with --test_arg=--app-cpu=<n>,
    # or switch the pinning off with --test_arg=--someipd-cpu=none --test_arg=--app-cpu=none.
    "--someipd-cpu=1",
    "--app-cpu=2",
    "--capture-cpu=3",
]

_BACKENDS = [
    # (target name suffix, plugin, args, data, target_compatible_with)
    (
        "_qemu",
        "@score_itf//score/itf/plugins:qemu_plugin",
        _QEMU_ARGS,
        [
            "config/qemu_config.json",
            ":_qemu_ifs",
        ],
        _QNX + ["@platforms//cpu:x86_64"],
    ),
    (
        "",
        "@score_itf//score/itf/plugins:hardware_plugin",
        _HARDWARE_ARGS,
        ["config/hardware_config.example.json"],
        _QNX,
    ),
]

# (suite, test module, host peer script, target app, config, extra args)
# The 4-subscriber variants exercise the daemon's unicast fan-out and take correspondingly longer.
# Each suite measures either latency or, in a run of its own, CPU and memory usage: the sampler slows the target, so it
# never runs while latency is measured.
_SUITES = [
    (
        "benchmark_{}{}".format(TRANSPORT, VARIANT),
        "test_someip_benchmark.py",
        "subscriber/someipy_subscriber.py",
        "bench_sender",
        "bench_{}_lgsomeip_config.json".format(TRANSPORT),
        [
            "--benchmark-transport=" + TRANSPORT,
            "--num-clients={}".format(NUM_CLIENTS),
            "--measure=" + MEASURE,
        ],
    )
    for TRANSPORT in [
        "udp",
        "tcp",
    ]
    for (VARIANT, NUM_CLIENTS, MEASURE) in [
        ("", 1, "latency"),
        ("_4clients", 4, "latency"),
        ("_resources", 1, "resources"),
    ]
] + [
    # RPC client processing time: the target calls Echo on a someipy server on the host, UDP only.
    (
        "benchmark_rpc",
        "test_someip_rpc.py",
        "server/someipy_echo_server.py",
        "rpc_client",
        "bench_rpc_lgsomeip_config.json",
        ["--benchmark-transport=udp"],
    ),
]

[
    py_itf_test(
        name = SUITE + SUFFIX,
        size = "enormous",
        srcs = [
            "conftest.py",
            PEER,
            TEST,
        ],
        args = _COMMON_ARGS + BACKEND_ARGS + SUITE_ARGS + [
            "--bench-app=$(location {}:{})".format(_BENCH, APP),
            "--bench-config=$(location {}:config/{})".format(_BENCH, CONFIG),
        ],
        data = BACKEND_DATA + [
            _DAEMON,
            "{}:{}".format(_BENCH, APP),
            "{}:config/{}".format(_BENCH, CONFIG),
        ],
        plugins = [PLUGIN],
        # A run owns the target and its raw capture, and its host-side peer binds fixed host ports (SD 30490, clients
        # from 60010), so two runs must not overlap.
        # Unsandboxed: the sandbox's user namespace does not own the host tap device qemu attaches to.
        tags = [
            "exclusive",
            "manual",
            "no-sandbox",
            "requires-network",
        ],
        target_compatible_with = COMPATIBLE,
        deps = [
            "//score/lg-someip/tests/benchmark/tools:benchmark_tools",
            "//score/lg-someip/tests/benchmark/tools:someip_bench",
            requirement("someipy"),
        ],
    )
    for (SUFFIX, PLUGIN, BACKEND_ARGS, BACKEND_DATA, COMPATIBLE) in _BACKENDS
    for (SUITE, TEST, PEER, APP, CONFIG, SUITE_ARGS) in _SUITES
]
