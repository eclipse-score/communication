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

Routing Control Codec Class Design
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

``RoutingControlCodec`` encodes and decodes the routing commands that the SD daemon sends to the routing
module over ``score::message_passing`` (service identifier ``someip_routing_module``). Commands flow in this
direction only; the protocol has no replies.

.. uml:: routing_control_codec_class_diagram.puml
   :align: center
   :alt: Routing Control Codec Class Diagram
   :width: 100%

``Message`` additionally holds ``remote_ip`` (``Ipv4Address``, four bytes in network byte order) and
``events`` and ``fields`` (``EventIds``, ``score::cpp::inplace_vector<std::uint16_t, kMaxEvents>``).

Functions
"""""""""

The public API (``routing_control_protocol.h``) consists of ``Encode`` and ``Decode``. Both are ``noexcept``,
``[[nodiscard]]`` and allocate no memory; ``Message`` holds at most ``kMaxEvents`` (64) event and field
identifiers each. The wire layout constants live in the internal header ``wire_format.h``.

- ``Encode(message, buffer)`` validates the message and writes it to ``buffer``. It returns the number of bytes
  written, or an ``ErrorCode`` without touching ``buffer``. ``kMaxMessageSize`` bytes always suffice.
- ``Decode(data)`` checks size, protocol version, opcode, reserved byte and identifier counts, then validates
  the decoded message.

Validation (internal) checks that the opcode is defined, that every parameter the opcode does not use is zero
or empty, and that every field identifier is one of the event identifiers.

Routing commands
""""""""""""""""

The SD daemon runs SOME/IP service discovery; the routing module only moves event notifications and does so
exclusively as instructed by these commands. They fall into two directions:

- **RX** (remote provider, local consumers): a *remote service* sets up the receive path of a remote service
  instance; a *local subscription* enables forwarding of one event of it to local consumers.
- **TX** (local provider, remote consumers): a *local service* sets up the send path of a local service
  instance; a *remote subscription* enables sending of one event of it to one remote endpoint.

A remote or local service alone transports no notification; it only prepares the data path for a service
instance. Notifications of an event flow once a local subscription (RX) or a remote subscription (TX) exists
for it.

.. list-table::
   :header-rows: 1
   :widths: 28 6 30 36

   * - Opcode
     - Dir.
     - Sent when
     - Routing module
   * - ``kAddRemoteService``
     - RX
     - a remote offer of the instance is found
     - receives on ``local_port`` from ``remote_ip:remote_port``, offers the instance with ``events`` to local
       consumers
   * - ``kRemoveRemoteService``
     - RX
     - the remote offer stops or expires
     - removes the remote service
   * - ``kAddLocalSubscription``
     - RX
     - the remote provider acknowledges the subscription of a local consumer
     - forwards received notifications of ``event`` to local consumers
   * - ``kRemoveLocalSubscription``
     - RX
     - the local subscription ends or expires
     - stops forwarding ``event``
   * - ``kAddLocalService``
     - TX
     - a local application offers the instance via SOME/IP
     - takes the instance's events locally and sends them from ``local_port`` with interface version ``major``;
       sends the current value of each of ``fields`` to new remote subscribers
   * - ``kRemoveLocalService``
     - TX
     - the local offer stops
     - removes the local service
   * - ``kAddRemoteSubscription``
     - TX
     - a remote consumer subscribes, and on every renewal
     - sends notifications of ``event`` to ``remote_ip:remote_port``; a renewal changes nothing
   * - ``kRemoveRemoteSubscription``
     - TX
     - the remote subscription ends or expires
     - stops sending ``event`` to that endpoint
   * - ``kShutdown``
     - both
     - the SD daemon shuts down gracefully
     - removes all remote services, local services and subscriptions

Parameters per opcode
"""""""""""""""""""""

``service`` and ``instance`` are used by every opcode except ``kShutdown``, which carries no parameters;
``x`` marks the additional parameters.

.. list-table::
   :header-rows: 1

   * - Opcode
     - event
     - local_port
     - remote_ip / remote_port
     - major
     - events
     - fields
   * - ``kAddRemoteService``
     -
     - x
     - x
     -
     - x
     -
   * - ``kRemoveRemoteService``
     -
     -
     -
     -
     -
     -
   * - ``kAddLocalSubscription``
     - x
     -
     -
     -
     -
     -
   * - ``kRemoveLocalSubscription``
     - x
     -
     -
     -
     -
     -
   * - ``kAddLocalService``
     -
     - x
     -
     - x
     - x
     - x
   * - ``kRemoveLocalService``
     -
     -
     -
     -
     -
     -
   * - ``kAddRemoteSubscription``
     - x
     -
     - x
     -
     -
     -
   * - ``kRemoveRemoteSubscription``
     - x
     -
     - x
     -
     -
     -
   * - ``kShutdown``
     -
     -
     -
     -
     -
     -

Wire layout
"""""""""""

Protocol version ``kProtocolVersion`` (1), multi-byte integers little endian, ``detail::kHeaderSize`` (22,
derived from the field offsets) bytes header followed by the identifier lists:

====== ====== ===================================================
Offset Size   Content
====== ====== ===================================================
0      1      version
1      1      opcode
2      2      service
4      2      instance
6      2      event
8      2      local port
10     2      remote port
12     4      remote IPv4 address, network byte order
16     1      major version
17     1      reserved, 0
18     2      event count n (at most 64)
20     2      field count m (at most 64)
22     2 * n  event identifiers
22+2n  2 * m  field identifiers
====== ====== ===================================================
