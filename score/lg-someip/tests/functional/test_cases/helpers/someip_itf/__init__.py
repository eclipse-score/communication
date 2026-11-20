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

"""SOME/IP test helpers: the codec, the DUT lifecycle, and the ETS and TC8 helper libraries.

Import each module by its full name. This file exports nothing.
"""

import pytest

# Helper asserts show their operands when they fail.
# The warning that this package is already imported is harmless.
pytest.register_assert_rewrite("someip_itf")
