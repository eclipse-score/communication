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

Routing Bridge
^^^^^^^^^^^^^^

The routing bridge connects the C UDP transport with the C++ routing module. It implements the
``someip_routing_*`` functions and contains no routing logic of its own; the TX direction will use the same bridge.

``RoutingBridge`` owns ``NotificationHandler`` and ``RoutingManager``. Routing commands arrive on message_passing
threads and only wake the data-path thread through ``someip_transport_wakeup()``. ``Poll()`` applies them and
``OnUdpDatagram()`` passes received datagrams to ``NotificationHandler``, both on the data-path thread, so routing state
needs no locking.

``someip_routing_stop()`` destroys the bridge when Linux stops or QNX cannot create the kernel thread.

.. uml:: routing_bridge_class.puml
   :align: center

.. uml:: routing_bridge_sequence.puml
   :align: center
