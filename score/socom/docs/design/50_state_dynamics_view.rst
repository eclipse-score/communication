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

State Dynamics View
===================

.. _event_subscription_state_machine:

Event subscription state
------------------------

The following model describes the state-behavior of an event subscription from a logical perspective.

.. uml:: models/state_diagram_event_subscription_state.puml
   :align: center
   :caption: State diagram of event subscription state

.. list-table:: States
   :header-rows: 1

   * - Name
     - Description
   * - Not_available
     - The client and server are not connected.
       No event subscription or transmission is possible.
   * - Available
     - The client and server are connected and the event is not subscribed.
       No event update will be received in this state.
   * - Event_subscribed
     - The user is subscribed to the event.
       Event updates will be received in this state.
       However, the server has not acknowledged the subscription yet.
       Thus clients do not know if the server will send event updates.

.. list-table:: Triggers
   :header-rows: 1

   * - Name
     - Description
   * - Client_connector::subscribe_event()
     - The user calls Client_connector::subscribe_event() in order to subscribe to an event.
   * - Client_connector::unsubscribe_event()
     - The user calls Client_connector::unsubscribe_event() in order to unsubscribe from an event.
   * - Client_connector::Callbacks::on_service_state_change(Service_state::available)
     - SOCom calls the user callback Client_connector::Callbacks::on_service_state_change(Service_state::available).
   * - Client_connector::Callbacks::on_service_state_change(!Service_state::available)
     - SOCom calls the user callback Client_connector::Callbacks::on_service_state_change() with a state different from Service_state::available.

.. _service_state_machine:

Service state
-------------

The following model describes the state-behavior of a service instance as reported to an instance of type Client_connector by callback on_state_change().

.. uml:: models/state_diagram_service_state.puml
   :align: center
   :caption: State diagram of service state

.. list-table:: States
   :header-rows: 1

   * - Name
     - Description
   * - Service_state::not_available
     - The service instance does not exist.
       No provided service instance within the service domain is known.
   * - Service_state::available
     - The service instance exists, is connected and available.
       A provided service instance is known, identified and enabled.
       Thus the service instance can be used and service requests will be answered.

.. list-table:: Triggers
   :header-rows: 1

   * - Name
     - Description
   * - Runtime::make_server_connector()
     - Construction of Disabled_server_connector by API function Runtime::make_server_connector().
   * - Disabled_server_connector::enable()
     - Disabling of Enabled_server_connector by API function Enabled_server_connector::disable().
   * - ~Disabled_server_connector()
     - Destruction of Disabled_server_connector().
   * - ~Enabled_server_connector()
     - Destruction of Enabled_server_connector().
