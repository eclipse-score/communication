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
from enum import Enum


class TestType(Enum):
    DISABLED_METHOD = "disabled_method_test"
    INCOMPLETE_HANDLERS = "incomplete_handlers_test"
    PROXY_RECREATION = "proxy_recreation_test"
    SKELETON_RECREATION = "skeleton_recreation_test"


def edge_cases_test(sut, test_type: TestType, timeout_sec=60):
    with sut.start_process(f"bin/edge_cases_test --test {test_type.value}",
                           cwd="/opt/EdgeCasesTestApp") as provider:
        assert provider.wait_for_exit(timeout_sec) == 0
