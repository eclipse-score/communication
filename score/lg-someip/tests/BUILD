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

load("//score/lg-someip:lgsomeip.bzl", "lgsomeip_cc_binary", "lgsomeip_cc_test")

_SOMEIP = "//score/lg-someip/src/someip"

_TEST_DEPS = ["@googletest//:gtest_main"]

# The tests run with the repository root as working directory.
_INTEGRATION_CONFIG_DIR = "score/lg-someip/tests/integration/someip/runtime"

lgsomeip_cc_test(
    name = "unittest_osabstraction_unit",
    size = "small",
    srcs = [
        "unit/osabstraction/socket/AddressTest.cpp",
        "unit/osabstraction/socket/IP4AddressTest.cpp",
        "unit/osabstraction/socket/IP6AddressTest.cpp",
        "unit/osabstraction/utils/log/LoggerTest.cpp",
        "unit/osabstraction/utils/time/TimerManagerTest.cpp",
    ],
    deps = [
        _SOMEIP + ":internal_headers",
        _SOMEIP + ":lgsomeip",
    ] + _TEST_DEPS,
)

lgsomeip_cc_test(
    name = "unittest_osabstraction_component",
    size = "small",
    srcs = [
        "component/osabstraction/socket/LocalAddressTest.cpp",
        "component/osabstraction/socket/NetworkDeviceTest.cpp",
        "component/osabstraction/socket/SocketTest.cpp",
    ],
    deps = [
        _SOMEIP + ":internal_headers",
        _SOMEIP + ":lgsomeip",
    ] + _TEST_DEPS,
)

lgsomeip_cc_test(
    name = "unittest_osabstraction_integration",
    size = "medium",
    srcs = [
        "integration/osabstraction/multiplex/MultiplexerTest.cpp",
        "integration/osabstraction/socket/Helper.cpp",
        "integration/osabstraction/socket/Helper.h",
        "integration/osabstraction/socket/TCPSocketTest.cpp",
        "integration/osabstraction/socket/UDPSocketTest.cpp",
    ],
    deps = [
        _SOMEIP + ":internal_headers",
        _SOMEIP + ":lgsomeip",
    ] + _TEST_DEPS,
)

lgsomeip_cc_test(
    name = "unittest_someip_unit",
    size = "small",
    srcs = glob(
        [
            "unit/someip/endpoint/*.cpp",
            "unit/someip/message/*.cpp",
            "unit/someip/packetrouter/*.cpp",
            "unit/someip/utils/**/*.cpp",
        ],
    ) + [
        "unit/someip/exception/ExceptionTest.cpp",
        "unit/someip/runtime/ApplicationManagerTest.cpp",
        "unit/someip/runtime/EventManagerTest.cpp",
        "unit/someip/runtime/ServiceManagerPrivateTest.cpp",
        "unit/someip/runtime/ServiceManagerTest.cpp",
    ],
    # TODO: Disabled for ThreadSanitizer because of data races and lock-order inversions in
    # ThreadPool, TimerMux, Socket, EventManager, ServiceManager and PacketRouterHost.
    # Re-enable once they are fixed.
    target_compatible_with = ["@score_cpp_policies//sanitizers/constraints:no_tsan"],
    deps = [
        "//score/lg-someip/daemon:lgsomeip-daemon-core",
        _SOMEIP + ":internal_headers",
        _SOMEIP + ":lgsomeip",
    ] + _TEST_DEPS,
)

lgsomeip_cc_test(
    name = "unittest_someip_component",
    size = "small",
    srcs = glob(["component/someip/config/*.cpp"]),
    local_defines = ["LGSOMEIP_TEST_CONFIG_DIR=\\\"" + _INTEGRATION_CONFIG_DIR + "\\\""],
    deps = [
        _SOMEIP + ":internal_headers",
        _SOMEIP + ":lgsomeip",
    ] + _TEST_DEPS,
)

lgsomeip_cc_test(
    name = "unittest_someip_integration",
    size = "large",
    timeout = "moderate",
    srcs = glob(["integration/someip/runtime/*.cpp"]),
    data = glob(["integration/someip/runtime/someip_config.json*"]),
    local_defines = ["LGSOMEIP_TEST_CONFIG_DIR=\\\"" + _INTEGRATION_CONFIG_DIR + "\\\""],
    # TODO: Disabled for ThreadSanitizer for the same reason as unittest_someip_unit.
    target_compatible_with = ["@score_cpp_policies//sanitizers/constraints:no_tsan"],
    deps = [
        "//score/lg-someip/daemon:lgsomeip-daemon-core",
        _SOMEIP + ":internal_headers",
    ] + _TEST_DEPS,
)

lgsomeip_cc_test(
    name = "unittest_serialization",
    size = "small",
    srcs = ["unit/serialization/someip/SomeIPParameterSerializationTest.cpp"],
    deps = ["//score/lg-someip:serialization"] + _TEST_DEPS,
)

[
    lgsomeip_cc_binary(
        name = app,
        srcs = ["latency/%s.cpp" % app],
        deps = [
            _SOMEIP + ":internal_headers",
            _SOMEIP + ":lgsomeip",
        ],
    )
    for app in [
        "request-app",
        "response-app",
    ]
]

test_suite(
    name = "all_tests",
    tests = [
        ":unittest_osabstraction_component",
        ":unittest_osabstraction_integration",
        ":unittest_osabstraction_unit",
        ":unittest_serialization",
        ":unittest_someip_component",
        ":unittest_someip_integration",
        ":unittest_someip_unit",
    ],
)
