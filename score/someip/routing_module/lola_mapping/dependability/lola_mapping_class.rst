..
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

LoLa Mapping
^^^^^^^^^^^^

The LoLa mapping is the contract between the routing module and LoLa applications for SOME/IP notifications. A
SOME/IP service instance is the LoLa service instance ``someip/<service>/<instance>``, each of its SOME/IP events the
LoLa event ``event_<event>``, with IDs as four-digit lowercase hex. ``LolaInstanceSpecifier()`` and
``LolaEventName()`` form these names; ``mw_com_config.json`` has to list every service instance and event in use.

Every event uses ``EventSample`` in both directions:

.. list-table:: EventSample
   :header-rows: 1
   :widths: 10 26 64

   * - Offset
     - Field
     - Meaning
   * - 0
     - ``size`` (uint32)
     - Payload size in bytes, at most ``kMaxPayloadSize`` (1400).
   * - 4
     - ``session`` (uint16)
     - Session ID of a received notification.
   * - 6
     - ``interface_version`` (uint8)
     - Interface version of a received notification.
   * - 7
     - ``message_type`` (uint8)
     - Message type of a received notification.
   * - 8
     - ``payload`` (1400 bytes)
     - SOME/IP payload, i.e. the bytes after the 16-byte SOME/IP header.

Transmitted samples only use ``size`` and ``payload``; the routing module creates their SOME/IP header.

.. uml:: lola_mapping_class.puml
   :align: center
   :alt: LoLa Mapping Class
