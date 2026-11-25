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

Deployment
""""""""""

On QNX, deploy ``mods-someip-routing.so`` under ``<prefix>/bin`` and load it with ``mount -T io-sock``. The module
runs inside io-sock and refuses unloading because it has no coordinated kernel-thread shutdown. On Linux, deploy and
run ``linux_routing_module`` under ``<prefix>/bin``; SIGINT and SIGTERM stop the standalone process.

Both deployments use ``<prefix>/etc/mw_com_config.json`` and an mw::log configuration file:

- The configuration directory is ``<prefix>/etc`` for a binary installed as ``<prefix>/bin/<binary>``
  (``ModuleConfigDirectory()``); the module finds its own path with ``dladdr()``.
- ``MW_LOG_CONFIG_FILE`` is set to ``logging.json`` in that directory unless already set, since mw::log otherwise
  looks next to the hosting executable. Logging configuration is process-wide and must be set before mw::log is first
  initialized. The module initializes mw::log before starting the transport.
- The C transport formats messages in a fixed-size buffer and forwards them to mw::log with their corresponding severity.
- mw::com is initialized with ``mw_com_config.json`` from that directory. A missing or unreadable file fails startup
  with ``ENOENT`` instead of terminating the host process.
- ``RoutingBridge`` starts receiving routing commands; a failure fails startup with its errno.

``someip_routing_stop()`` destroys the bridge when Linux stops or QNX cannot create the kernel thread.

.. uml:: routing_bridge_class.puml
   :align: center

.. uml:: routing_bridge_sequence.puml
   :align: center
