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
"""Replacement for the score_tooling ``attribute_plugin``. The test suites import it without changes."""

# ponytail: drops the traceability properties; switch to @score_itf//score/itf/plugins:attribute_plugin once
# integration_test() accepts extra plugins.


def add_test_properties(**_properties):
    """Return the test unchanged."""
    return lambda test: test
