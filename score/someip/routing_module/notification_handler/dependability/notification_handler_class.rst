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

Notification Handler
^^^^^^^^^^^^^^^^^^^^

``NotificationHandler`` moves SOME/IP notifications between UDP, received and sent through the platform transport, and
LoLa. It implements ``IRoutingCommandHandler``, is only called on the data path thread and needs no locking. Commands
of the TX direction are rejected until it is supported.

The receive path of a remote service consists of its UDP route (service ID, local UDP port and remote endpoint) and
its LoLa service instance; it is active while the LoLa service instance is offered.

- ``AddRemoteService()`` opens the local UDP port and offers the remote service in LoLa, with one ``EventSample``
  LoLa event per SOME/IP event, named as described in the LoLa mapping. The SOME/IP events of the first command for
  an instance define its LoLa events. Repeating the command for an active receive path updates its UDP route.
- The command is rejected without changing the routing state if a UDP port is 0, the local UDP port cannot be opened,
  the LoLa service instance cannot be created (e.g. it is missing in ``mw_com_config.json``) or offered, or another
  active instance of the service uses the same UDP route. A SOME/IP notification carries no instance ID, so its UDP
  route has to identify one instance. Opened UDP ports stay open.
- ``RemoveRemoteService()`` stops the LoLa offer and removes the local subscriptions of the instance. Its LoLa
  skeleton is kept and offered again by a later ``AddRemoteService()``. ``RemoveAll()`` does this for every remote
  service.
- ``AddLocalSubscription()`` and ``RemoveLocalSubscription()`` switch the forwarding of one SOME/IP event of an active
  receive path; repeating them changes nothing. Subscriptions of unknown instances or events are rejected.

``OnUdpDatagram()`` parses a received UDP datagram with ``SomeipDatagramParser`` and forwards each notification whose
service ID, local UDP port and sender endpoint match an active receive path and whose event is locally subscribed:

- It allocates a sample of the LoLa event, sets payload length, session ID, interface version and message type, and
  copies the payload from the packet storage into the sample. QNX reads the mbuf chain directly; Linux receives into a
  userspace buffer first.
- Every dropped message is logged. A non-notification message, a missing receive path and an unknown or not locally
  subscribed event are logged as warning. A payload larger than ``kMaxPayloadSize``, no free LoLa sample slot and a
  failed payload copy or LoLa send are logged as error; they only happen in a misconfigured or faulty system, e.g. with
  too few LoLa sample slots.
- A malformed message stops parsing; the rest of the datagram is dropped and a warning is logged.

Destroying an offered LoLa skeleton does not stop its LoLa offer, so ``NotificationHandler`` stops all LoLa offers on
destruction.

.. uml:: notification_handler_class.puml
   :align: center
   :alt: Notification Handler Class
