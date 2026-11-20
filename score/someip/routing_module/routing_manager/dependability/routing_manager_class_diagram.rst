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

Routing Manager Class Design
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

``RoutingManager`` receives the routing commands of the SD daemon via ``score::message_passing`` (service
identifier ``someip_routing_module``, see the routing control protocol) and applies them to the data path
through the ``IRoutingCommandHandler`` interface, which ``EventHandler`` implements. The interface decouples the
control path from the data path, so that ``RoutingManager`` is tested without network or LoLa.

.. uml:: routing_manager_class_diagram.puml
   :align: center
   :alt: Routing Manager Class Overview
   :width: 60%

.. uml:: routing_manager_class.puml
   :align: center
   :alt: RoutingManager

.. uml:: routing_command_handler_class.puml
   :align: center
   :alt: IRoutingCommandHandler and its types

``Endpoint`` additionally holds the IPv4 ``address``. ``RoutingManager::Start()`` returns
``score::cpp::expected_blank<score::os::Error>``.

Threads
"""""""

- ``score::message_passing`` callbacks (connect, disconnect, command) decode each command and append it to a
  bounded queue of ``kCommandQueueCapacity`` entries, then call the wakeup callback. They never block.
- The data path thread calls ``ProcessPendingCommands()`` after a wakeup and before forwarding further samples.
  It applies the queued entries in arrival order to the ``IRoutingCommandHandler``, which is therefore only
  called from that thread.

The queue is protected by a mutex and has fixed storage, so receiving commands does not allocate.

Behavior
""""""""

- **Single client:** only one SD daemon connection is accepted; further connection attempts are rejected while
  it is connected.
- **Invalid commands:** commands that fail to decode are logged and discarded; commands rejected by the handler
  are logged. The connection is kept in both cases.
- **Queue overflow:** each command received while the queue is full is discarded and logged. The queue capacity
  has to cover the SD daemon's startup burst (assumption of use towards integration).
- **Shutdown:** ``kShutdown`` is applied in order and removes all routing state (``RemoveAll()``).
- **Disconnect:** the queue is cleared and the removal of all routing state is queued in its place, so commands of
  a reconnected SD daemon are always applied after it. Connect, disconnect and announced shutdown are logged as
  warnings, so they stay visible at the default log level.

Sequences
"""""""""

.. uml:: routing_manager_command_sequence.puml
   :align: center
   :alt: Routing command processing

.. uml:: routing_manager_disconnect_sequence.puml
   :align: center
   :alt: SD daemon disconnect and reconnect
