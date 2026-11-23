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

io-sock Transport
^^^^^^^^^^^^^^^^^

The io-sock transport runs the routing module's notification transport inside io-sock, the QNX network stack. Its
C part, ``iosock_module.c``, is a FreeBSD kernel module. The C interface ``iosock_transport.h`` connects it to the
routing module, which implements the ``someip_routing_*`` functions.

- Loading the module starts the routing module and creates one kernel thread. Unloading is refused, because there is
  no coordinated stop of the kernel thread.
- ``someip_iosock_open_udp_socket()`` opens a kernel UDP socket on a port for all local IPv4 addresses. Sockets are
  never closed; opening an open port succeeds.
- The receive upcall runs in the network stack with the receive buffer locked. It only marks the socket pending and
  wakes the kernel thread.
- The kernel thread calls ``someip_routing_poll()``, drains each pending socket with non-blocking ``soreceive()``,
  at most 32 datagrams per socket and wakeup, and then waits for the next wakeup, at most 1 s.
- Each datagram holding at least a SOME/IP header is passed as mbuf chain to ``someip_routing_datagram()`` and freed
  when the call returns. Truncated and short datagrams are logged and dropped.
- ``someip_iosock_wakeup()`` wakes the kernel thread from any thread through a POSIX semaphore. Wakeups while the
  thread is awake coalesce.

``IoSockPacketReader`` gives the routing module in-place access to a received datagram, e.g. for
``SomeipDatagramParser``. ``Copy()`` copies a byte range with ``m_copydata()`` directly from the mbuf chain into the
destination, e.g. a LoLa sample slot. It returns false without reading if the range does not lie within the packet.
The reader is valid only during ``someip_routing_datagram()``; its calls are statically dispatched.

.. uml:: iosock_transport_class.puml
   :align: center
   :alt: io-sock Transport Class

Sequences
"""""""""

.. uml:: iosock_transport_receive_sequence.puml
   :align: center
   :alt: Receiving datagrams in the io-sock module
