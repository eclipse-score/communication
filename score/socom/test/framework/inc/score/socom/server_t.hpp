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

#ifndef SOCOM_SERVER_T_HPP
#define SOCOM_SERVER_T_HPP

#include <cstddef>

#include "score/socom/connector_factory.hpp"
#include "score/socom/event.hpp"
#include "score/socom/method.hpp"
#include "score/socom/payload.hpp"
#include "score/socom/posix_credentials.hpp"
#include "score/socom/server_connector.hpp"
#include "score/socom/socom_mocks.hpp"

namespace score::socom {

/// \brief Facade for the server connector and callback mocks.
///
/// It allows easy configuration of mocks and blocks its destruction until all
/// expectations have been fulfilled. The stored server connector is always
/// enabled and ready for communication after construction.
struct Server_data {
   private:
    std::atomic<bool> m_callback_called{true};
    std::atomic<bool> m_method_callback_called{true};
    std::atomic<bool> m_subscribed{true};
    std::atomic<bool> m_unsubscribed{true};
    std::atomic<bool> m_method_payload_allocate_called{true};
    std::atomic<uint32_t> m_num_method_calls{0};
    Server_connector_callbacks_mock m_callbacks;
    Server_connector_credentials_callbacks_mock m_credential_callbacks;
    Enabled_server_connector::Uptr m_connector;

   public:
    /// \brief Create a new instance with the configuration stored in factory
    ///
    /// \param[in] factory factory to create server connector with
    explicit Server_data(Connector_factory& factory);

    /// \brief Create new instance and expect immediately a method call.
    ///
    /// \param[in] factory factory to create server connector with
    /// \param[in] method_id method which is expected
    /// \param[in] payload input of the method
    Server_data(Connector_factory& factory, Method_id method_id, Payload const& expected_payload);

    /// \brief Create a new instance with the configuration stored in factory
    ///
    /// \param[in] factory factory to create server connector with
    /// \param[in] configuration use this instead of the one stored in factory
    /// \param[in] instance use this instead of the one stored in factory
    Server_data(Connector_factory& factory,
                Server_service_interface_definition const& configuration,
                Service_instance const& instance);

    /// \brief Create a new instance with the configuration stored in factory and POSIX credentials
    ///
    /// \param[in] factory factory to create server connector with
    /// \param[in] configuration use this instead of the one stored in factory
    /// \param[in] instance use this instead of the one stored in factory
    /// \param[in] credentials use POSIX credentials
    Server_data(Connector_factory& factory,
                Server_service_interface_definition const& configuration,
                Service_instance const& instance, Posix_credentials const& credentials);

    Server_data(Server_data const&) = delete;
    Server_data(Server_data&&) = delete;

    /// \brief Block until all expectations have been fulfilled
    ~Server_data();

    Server_data& operator=(Server_data const&) = delete;
    Server_data& operator=(Server_data&&) = delete;

    /// \brief Return callback mocks object
    ///
    /// \return callback mocks
    Server_connector_callbacks_mock& get_callbacks();

    /// \brief Return credentials callback mocks object
    ///
    /// \return callback mocks
    Server_connector_credentials_callbacks_mock& get_credentials_callbacks();

    /// \brief Return server connector
    ///
    /// \return server connector
    Enabled_server_connector& get_connector();

    /// \brief Disable server connector and move it out of Server_data
    ///
    /// The caller is then responsible of taking care of the returned server
    /// connector, but the mocks stored in Server_data are still bound to the
    /// server connector.
    ///
    /// \return disabled server connector
    Disabled_server_connector::Uptr disable();

    /// \brief Enable server connector and move it into Server_data
    ///
    /// The only object allowed to be moved in is the one returned from
    /// disable(). Otherwise strange effects with configuring mocks will happen.
    ///
    /// \param[in] disabled_connector connector to be enabled
    void enable(Disabled_server_connector::Uptr disabled_connector);

    /// \brief Send event update to subscribed clients
    ///
    /// \param[in] event_id the event update
    /// \param[in] payload the data to send
    void update_event(Event_id const& event_id, Payload const& payload);

    /// \brief Send requested event update to subscribed and requesting clients
    ///
    /// \param[in] event_id the event update
    /// \param[in] payload the data to send
    void update_requested_event(Event_id const& event_id, Payload const& payload);

    /// \brief Expect a call to the on_event_subscription_change callback
    ///
    /// \param[in] event_id the event to which a client subscribes
    /// \param[in] state the subscription state
    /// \param[in] subscription_change_callback callback will be called when the expectation is
    /// fulfilled
    /// \return boolean reference which becomes true when the callback is called
    std::atomic<bool> const& expect_on_event_subscription_change(
        Event_id const& event_id, Event_state const& state,
        Event_subscription_change_callback subscription_change_callback = {});

    /// \brief Expect (nonblocking) the subscription/unsubscription of an event
    void expect_event_subscription(Event_id const& event_id);

    /// \brief Expect a call to the on_event_subscription_change callback - without synchronization
    /// return
    void expect_on_event_subscription_change_nosync(Event_id const& event_id,
                                                    Event_state const& state);

    /// \brief Expect but not respond to an update event request
    ///
    /// \param[in] event_id event for which a update is requested
    /// \return boolean reference which becomes true when the callback is called
    std::atomic<bool> const& expect_update_event_request(Event_id const& event_id);

    /// Expect but not respond to an update event request
    ///
    /// There can be many update requests and the atomic becomes true after the first one has been
    /// received.
    ///
    /// \param[in] event_id event for which a update is requested
    /// \return boolean reference which becomes true when the callback is called
    std::atomic<bool> const& expect_update_event_requests(Event_id const& event_id);

    /// \brief Expect and respond to an update event request
    ///
    /// \param[in] event_id event for which a update is requested
    /// \param[in] payload the data to send
    void expect_and_respond_update_event_request(Event_id const& event_id, Payload const& payload);

    /// \brief Expect and respond to method calls
    ///
    /// \param[in] method_id method which is expected
    /// \param[in] result allocated payload
    /// \return boolean reference which becomes true when the callback is called
    std::atomic<bool> const& expect_method_allocate_payload(Method_id const& method_id,
                                                            score::Result<Writable_payload> result);

    /// \brief Expect and respond to method calls
    ///
    /// \param[in] counter number of expected method calls
    /// \param[in] method_id method which is expected
    /// \param[in] payload input of the method
    /// \param[in] result return of the method
    /// \return boolean reference which becomes true when the callback is called
    std::atomic<bool> const& expect_and_respond_method_calls(size_t counter,
                                                             Method_id const& method_id,
                                                             Payload const& payload,
                                                             Method_result const& result);

    /// \brief Expect and respond to a method call
    ///
    /// \param[in] method_id method which is expected
    /// \param[in] payload input of the method
    /// \param[in] result return of the method
    /// \return boolean reference which becomes true when the callback is called
    std::atomic<bool> const& expect_and_respond_method_call(Method_id const& method_id,
                                                            Payload const& payload,
                                                            Method_result const& result);

    /// \brief Expect method call and return received client callback
    ///
    /// \param[in] method_id method which is expected
    /// \param[in] payload input of the method
    /// \return Future which will return the client callback once the method is called
    std::future<Method_call_reply_data_opt> expect_and_return_method_call(
        Method_id const& method_id, Payload const& payload);

    /// Expect minimum number of method call and return received client callback
    ///
    /// \param[in] method_id method which is expected
    /// \param[in] payload input of the method
    /// \return Future which will return the client callback once the method is called
    std::future<void> expect_method_calls(std::size_t const& min_num, Method_id const& method_id,
                                          Payload const& payload);

    /// \brief Return event mode
    ///
    /// \param[in] server_id event for which the mode is requested
    /// \return the mode of the event
    Event_mode get_event_mode(Event_id server_id) const;
};

}  // namespace score::socom

#endif
