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

The transport requires UDP support for ``SO_PEEK_OFF`` and fails initialization if it is unavailable. It peeks the
datagram length and sender, then leaves the datagram in the socket receive queue while the packet reader uses
``SO_PEEK_OFF`` and ``MSG_PEEK`` to copy headers and selected payloads directly into their destinations. The transport
consumes the datagram after the routing callback returns.

Short datagrams are dropped and logged at most once per second, with the number suppressed reported by the next log.
Recoverable UDP socket errors are logged and the affected socket is retried after 100 ms; other sockets and wakeups
remain active. Invalid descriptors and unrecoverable poll or socket errors stop the process.

.. uml:: linux_transport_sequence.puml
   :align: center
   :alt: Linux UDP transport receive sequence