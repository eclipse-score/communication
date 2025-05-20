# *******************************************************************************
# Copyright (c) 2025 Contributors to the Eclipse Foundation
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

load("//score/tools/sctf/bazel_gen:sctf_gen.bzl", "py_sctf_test")

py_sctf_test(
    name = "provider_restart_max_subscribers_no_proxy_test",
    srcs = [
        "__init__.py",
        "provider_restart_max_subscribers_no_proxy_test.py",
        "provider_restart_max_subscribers_test_fixture.py",
    ],
    data = ["//score/mw/com/test/partial_restart/provider_restart_max_subscribers:provider_restart_max_subscribers-pkg.tar"],
    main = "provider_restart_max_subscribers_no_proxy_test.py",
    module_root = "score/mw/com/test/partial_restart/provider_restart_max_subscribers/sct",
)

py_sctf_test(
    name = "provider_restart_max_subscribers_with_proxy_test",
    srcs = [
        "__init__.py",
        "provider_restart_max_subscribers_test_fixture.py",
        "provider_restart_max_subscribers_with_proxy_test.py",
    ],
    data = ["//score/mw/com/test/partial_restart/provider_restart_max_subscribers:provider_restart_max_subscribers-pkg.tar"],
    main = "provider_restart_max_subscribers_with_proxy_test.py",
    module_root = "score/mw/com/test/partial_restart/provider_restart_max_subscribers/sct",
)
