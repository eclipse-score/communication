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
"""Network constants and service IDs for the TC8 conformance tests.

All TC8 tests and helpers get port numbers and multicast addresses from this module.

Each test target runs in its own process and can set its own ports through environment variables.
The module reads these variables when it is imported.
If a variable is not set, the module uses the default value.
"""

import os

#: SOME/IP-SD port (UDP). The DUT and the tester both bind to this port.
#: The SOME/IP-SD stack drops SD packets from all other source ports.
#: Read from ``TC8_SD_PORT``. The default is 30490, the well-known SOME/IP-SD port.
SD_PORT: int = int(os.environ.get("TC8_SD_PORT", "30490"))

#: SOME/IP-SD multicast group address. All SOME/IP nodes join this group.
#: Read from ``TC8_SD_MULTICAST_ADDR``. The default is the group of the DUT configuration.
SD_MULTICAST_ADDR: str = os.environ.get("TC8_SD_MULTICAST_ADDR", "224.244.224.245")

#: DUT unreliable (UDP) service port. It is the ``unreliable`` port of the service in the DUT deployment.
#: Read from ``TC8_SVC_PORT``. The default is 30509.
DUT_UNRELIABLE_PORT: int = int(os.environ.get("TC8_SVC_PORT", "30509"))

#: DUT reliable (TCP) service port. It is the ``reliable`` port of the service in the DUT deployment.
#: Read from ``TC8_SVC_TCP_PORT``. The default is 30510.
DUT_RELIABLE_PORT: int = int(os.environ.get("TC8_SVC_TCP_PORT", "30510"))

#: UDP port of the event multicast group. It is the event multicast port of the DUT deployment.
#: Read from ``TC8_MULTICAST_EVENT_PORT``. The default is 30511.
MULTICAST_EVENT_PORT: int = int(os.environ.get("TC8_MULTICAST_EVENT_PORT", "30511"))

# Tc8Interface, the service that the DUT offers under ``--offer tc8``.
SERVICE_ID: int = 0x1234
INSTANCE_ID: int = 0x5678
MAJOR_VERSION: int = 0x01
MINOR_VERSION: int = 0x00000000
#: The eventgroup delivered unicast.
EVENTGROUP_ID: int = 0x4455
#: The eventgroup delivered on the event multicast group.
MULTICAST_EVENTGROUP_ID: int = 0x4465
#: Echo. All suites use this method to make sure that the DUT still answers.
METHOD_ID: int = 0x0421
#: The cyclic offer delay of the deployment, between two offers of the Main Phase.
CYCLIC_OFFER_DELAY_MS: float = 2000.0
#: A service that the DUT never offers. A Subscribe for it gets an immediate NAck. A Find for it gets no offer.
UNKNOWN_SERVICE_ID: int = 0xDEAD
#: The subscriber port in the endpoint options of malformed or flood Subscribes. Nothing listens on it.
PROBE_SUBSCRIBER_PORT: int = 34567
