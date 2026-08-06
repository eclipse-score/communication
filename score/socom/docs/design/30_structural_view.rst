..
   # *******************************************************************************
   # Copyright (c) 2025 Contributors to the Eclipse Foundation
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

Structural View
===============

The bundle service oriented communication (SOCom) implements components for client/server pattern based communication for the following communication paradigms:

- remote procedure call
- publish/subscribe (event communication)

SOCom provides interfaces which enable the implementation of service gateways (bridges).
This allows users to independently implement service bridges for different communication protocols (e.g. IPC, SOME/IP, …).
SOCom provides intra-process client/server communication only.

.. uml:: models/component_diagram_socom.puml
   :align: center
   :caption: Component diagram of SOCom

**Software elements**

- :ref:`runtime_component`
- :ref:`client_connector_component`
- :ref:`server_connector_component`

.. _runtime_component:

Runtime
-------

The Runtime creates and connects Client_connectors and Server_connectors.
In addition to that it has a plugin interface for adding bridges to cross IPC or network boundaries.
The Runtime must outlive all created Client_connectors and Server_connectors.

.. _client_connector_component:

Client connector
----------------

A client application owns and uses this component to join client/server pattern based service oriented communication (SOCom).
The client connector interacts with the server connector in order perform the supported communication primitives.
The client connector API provides the following features:

- service instance state change indications
- asynchronous remote procedure call:

  - with method reply
  - without method reply (fire and forget)

- event subscription:

  - with optional initial value request (required for fields)

- event subscription acknowledge indication
- event update indication
- requested event update indication

.. _server_connector_component:

Server connector
----------------

The Server_connector represents a service, which is used by a Client_connector.
It can send event updates and respond to method calls.
