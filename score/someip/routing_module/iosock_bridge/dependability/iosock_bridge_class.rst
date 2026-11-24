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

io-sock Bridge
^^^^^^^^^^^^^^

The io-sock bridge connects the io-sock module, written in C, with the C++ routing module. It implements the
``someip_routing_*`` functions of the io-sock transport interface and contains no routing logic of its own; the TX
direction will use the same bridge.

``IoSockBridge`` owns ``NotificationHandler`` and ``RoutingManager``. Routing commands arrive on message_passing
threads and only wake the module's kernel thread through ``someip_iosock_wakeup()``. ``Poll()`` applies them and
``OnUdpDatagram()`` passes received datagrams to ``NotificationHandler``, both on the kernel thread, so routing state
needs no locking.

When io-sock loads the module:

- The configuration directory is ``<prefix>/etc`` for a module installed as ``<prefix>/bin/<module>``
  (``ModuleConfigDirectory()``); the module finds its own path with ``dladdr()``.
- ``MW_LOG_CONFIG_FILE`` is set to ``logging.json`` in that directory unless already set, since mw::log otherwise
  looks next to the io-sock binary. The module initializes mw::log before starting the transport; the provided
  configuration selects the linked QNX slog2 backend with ``kSystem``. Logging configuration is process-wide and must
  be set before mw::log is first initialized.
- The C transport formats messages in a fixed-size buffer and forwards them to mw::log with their corresponding severity.
- mw::com is initialized with ``mw_com_config.json`` from that directory. A missing or unreadable file fails the
  module load with ``ENOENT`` instead of terminating io-sock.
- ``IoSockBridge`` starts receiving routing commands; a failure fails the module load with its errno.

``someip_routing_abort_start()`` destroys the bridge if io-sock cannot create the kernel thread.

.. uml:: iosock_bridge_class.puml
   :align: center
   :alt: io-sock Bridge Class

.. uml:: iosock_bridge_sequence.puml
   :align: center
   :alt: io-sock Bridge Sequence
