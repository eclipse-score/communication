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

from abc import ABC, abstractmethod
from typing import Tuple


class SystemUnderTest(ABC):
    """
    Abstract class that builds the interface, that every environment has to implement.
    The tests then only interact with the system under test through this interface.
    This way, we can ensure that the same test will work in multiple environments.
    """

    @abstractmethod
    def execute(self, command: str) -> Tuple[int, str]:
        """Executes a command on the serial interface of the system under test"""
        pass
