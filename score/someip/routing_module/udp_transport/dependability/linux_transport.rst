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

Linux UDP Transport
^^^^^^^^^^^^^^^^^^^

The Linux UDP transport provides the same packet, socket-opening and wakeup C interface as the QNX transport.
``someip_linux_transport_init()`` creates the wakeup descriptor. The data-path thread calls
``someip_linux_transport_poll()`` to apply pending commands, wait for UDP sockets or a wakeup, and pass received
datagrams to the routing bridge. ``linux_routing_module`` runs the bridge and transport in a standalone process and
stops them on SIGINT or SIGTERM.

Each datagram is received into a temporary buffer. The packet reader copies selected bytes from that buffer into the
LoLa sample. This Linux path does not provide the QNX mbuf-to-LoLa direct-copy property.

.. uml:: linux_transport_sequence.puml
   :align: center
   :alt: Linux UDP transport receive sequence