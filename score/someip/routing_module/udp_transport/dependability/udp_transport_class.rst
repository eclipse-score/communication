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

UDP Transport
^^^^^^^^^^^^^

The UDP transport provides the C interface ``udp_transport.h`` to the C++ routing bridge. The build selects either
the Linux socket implementation or the QNX kernel-socket implementation. ``UdpPacketReader`` reads a received packet
through that interface and is valid only during ``someip_routing_datagram()``; its calls are statically dispatched.

QNX transport
"""""""""""""

The QNX transport runs inside io-sock, the QNX network stack. Its module registration and kernel-socket transport are
implemented in ``qnx/module.c`` and ``qnx/transport.c`` respectively.

- Loading the module starts the routing module and creates one kernel thread. Unloading is refused, because there is
  no coordinated stop of the kernel thread.
- ``someip_transport_open_udp_socket()`` opens a kernel UDP socket on a port for all local IPv4 addresses. Sockets are
  never closed; opening an open port succeeds.
- The receive upcall runs in the network stack with the receive buffer locked. It only marks the socket pending and
  wakes the kernel thread.
- The kernel thread calls ``someip_routing_poll()``, drains each pending socket with non-blocking ``soreceive()``,
  at most 32 datagrams per socket and wakeup, and then waits for the next wakeup, at most 1 s.
- Each datagram holding at least a SOME/IP header is passed as mbuf chain to ``someip_routing_datagram()`` and freed
  when the call returns. Truncated and short datagrams are dropped and logged at most once per reason per second;
  the next log reports the number suppressed.
- ``someip_transport_wakeup()`` wakes the kernel thread from any thread through a POSIX semaphore. Wakeups while the
  thread is awake coalesce.
- C transport messages use mw::log through the routing bridge, with messages limited to 255 bytes.

On QNX, ``Copy()`` uses ``m_copydata()`` to copy directly from the mbuf chain into the destination, e.g. a LoLa sample
slot. It returns false if the range does not lie within the packet or the transport reports a read failure.

.. uml:: udp_transport_class.puml
   :align: center

Sequences
"""""""""

.. uml:: udp_transport_receive_sequence.puml
   :align: center
   :alt: Receiving datagrams in the io-sock module
