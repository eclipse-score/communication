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

from test_fixture import consumer, provider

# TODO: Implement once set_get_and_notifier mode is supported by the provider and consumer binaries.
# Scenarios to cover (same as set_and_notifier, but also verify result of getter):
# 1. calling set with valid value -> calling get and GetNewSamples both return value set with setter
# 2. calling set with invalid value (set handler clamps the value) -> calling get and GetNewSamples both return clamped value
# 3. calling Update / send -> calling get and GetNewSamples both return value set with send
