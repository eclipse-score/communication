/********************************************************************************
 * Copyright (c) 2025 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#ifndef SOCOM_TEMPORARY_EVENT_SUBSCRIPTION_HPP
#define SOCOM_TEMPORARY_EVENT_SUBSCRIPTION_HPP

#include "score/socom/client_connector.hpp"
#include "score/socom/socom_mocks.hpp"

namespace score::socom {

/// \brief Temporary_event_subscriptions keeps a client to an event subscribed
///         until destruction
class Temporary_event_subscription {
    Event_id m_event_id;
    Client_connector& m_cc;

   public:
    /// \brief Behave in ways not expected by the middleware
    enum class Brokenness {
        /// \brief the server does not answer update event request, when
        /// Event_mode::update_and_initial is used
        no_server_reponse,
        /// \brief the server does not expect an on_event_update-request and
        /// does not answer update event request, when
        /// Event_mode::update_and_initial is used
        no_server_reponse_second_requester
    };

    /// \brief Subscribe with Event_mode::update_and_initial_value but without server_response
    ///
    /// \param[in] cc client connector which subscribes to an event
    /// \param[in] sc_callbacks callbacks of the server connector providing the event
    /// \param[in] event_id the event to which shall be subscribed
    /// \param[in] brokeness behave in ways not expected by the middleware
    Temporary_event_subscription(Client_connector& cc,
                                 Server_connector_callbacks_mock& sc_callbacks,
                                 Event_id const& event_id, Brokenness const& brokenness);

    /// \brief Subscribe with Event_mode::update
    ///
    /// \param[in] cc client connector which subscribes to an event
    /// \param[in] event_id the event to which shall be subscribed
    Temporary_event_subscription(Client_connector& cc, Event_id const& event_id);

    Temporary_event_subscription(Temporary_event_subscription const&) = delete;
    Temporary_event_subscription(Temporary_event_subscription&&) = delete;

    /// \brief Unsubscribe from the event
    ~Temporary_event_subscription();

    Temporary_event_subscription& operator=(Temporary_event_subscription const&) = delete;
    Temporary_event_subscription& operator=(Temporary_event_subscription&&) = delete;
};

using Subscriptions = std::vector<std::unique_ptr<Temporary_event_subscription>>;

}  // namespace score::socom

#endif
