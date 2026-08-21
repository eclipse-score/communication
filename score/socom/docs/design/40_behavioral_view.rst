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

Behavioral View
===============

This component implements the following state machines:

- :ref:`event subscription state <event_subscription_state_machine>`
- :ref:`service state <service_state_machine>`

Runtime
-------

The runtime is a (process internal) service instance broker.
Client connectors and server connectors are created using the runtime (factory), which keeps track of their existence and state.
Depending on the existence and state of the communication partners, the runtime takes care for connecting/disconnecting service interface and service instance compatible client and server connectors.
Additionally, it provides a service instance find interface.

Server connector
----------------

A server application owns and uses this component to join client/server pattern based service oriented communication (SOCom).
The server connector interacts with the client connector in order perform the supported communication primitives.
The server connector API provides the following features:

- remote procedure called indication
- event subscription state changed indication:

  - on first subscriber
  - on last unsubscriber

- event update request indication
- event mode getter (event with/without initial update)
- update event
- update requested event
- acknowledge event subscription

Method Communication
--------------------

Methods calls are routed 1:1 from client to server application.
If a method reply is requested, the calling client is served with the related reply.
Independent method calls share no state.

.. uml:: models/interaction_diagram_method_communication.puml
   :align: center
   :caption: Interaction diagram: Method communication

Event Communication
-------------------

The client subscribes to an event.
On the subscription, SOCom informs the server using the request for events.
If no client is subscribed to an event, the server will not send any event updates.

After unsubscription of the subscriber, SOCom informs the server that an event is not subscribed anymore.
SOCom keeps track of subscription states and related clients.

.. uml:: models/interaction_diagram_event_communication.puml
   :align: center
   :caption: Interaction diagram: Event communication

Field Notification Communication
---------------------------------

A field notification is implemented as an event with event update on subscription.

.. note::

   Additional behavior compared to field communication:
   since SOCom does not save the current event values, SOCom requests an event update from the server for each new subscriber.

Servers answer this request with update_requested_event().
Those updates are indicated to the client which is subscribed and has an initial event update pending.

Regular update_event() calls update for the events of the subscriber and also fulfills the initial event update use-case.
Consequently, update_event() satisfies the event update request implicitly.

.. uml:: models/interaction_diagram_field_notification_communication.puml
   :align: center
   :caption: Interaction diagram: Field notification communication

Service Gateway - require service
----------------------------------

The creation of client connectors is forwarded to bridges as request_service() calls.
Service bridges look up the required service instance within their domain.
If available, they connect to the remote counterpart and locally create a proxy server connector (forwarding all communication).

.. uml:: models/interaction_diagram_service_gateway_require_service.puml
   :align: center
   :caption: Interaction diagram: Service Gateway - require service

Service Gateway - provide service
----------------------------------

The creation of server connectors is found by service bridges using the client connector Service_state_change_callback.
Service bridges provide this information within their domain.
If any domain partner requests the service instance, the service bridge forwards all communication using this proxy client connector.

.. uml:: models/interaction_diagram_service_gateway_provide_service.puml
   :align: center
   :caption: Interaction diagram: Service Gateway - provide service

Deadlock detection
------------------

In order to facilitate deadlock detection, before a callback is called by a Client_connector or
Server_connector the thread id of the caller is saved.
After the callback returns, the previously saved thread id is removed.
If the calling object is destructed prematurely the deadlock is detected by checking if the thread
id is still present, issuing a warning log and terminating the application.

Client_connector deadlocks
---------------------------

When a running callback destroys the calling Client_connector a deadlock will happen, which will cause the application to be terminated.

The deadlock is caused because the Client_connector destructor waits for the callback to return.
The callback on the other hand waits for the destructor to return.

.. uml:: models/interaction_diagram_client_connector_deadlocks.puml
   :align: center
   :caption: Interaction diagram: Client_connector deadlocks

Server_connector deadlocks
---------------------------

When a running callback destroys the calling Server_connector a deadlock will happen, which will cause the application to be terminated.

The deadlock is caused because the Server_connector destructor waits for the callback to return.
The callback on the other hand waits for the destructor to return.

.. uml:: models/interaction_diagram_server_connector_deadlocks.puml
   :align: center
   :caption: Interaction diagram: Server_connector deadlocks
