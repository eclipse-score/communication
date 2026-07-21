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
def test_edge_cases(target):
    args = ["--service_instance_manifest", "./etc/mw_com_config.json"]
    with target.wrap_exec("bin/proxy_recreation_test", args, cwd="/opt/EdgeCasesTestApp", wait_on_exit=True):
        pass
