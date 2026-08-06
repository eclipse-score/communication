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

#ifndef SOCOM_CLIENTS_T_HPP
#define SOCOM_CLIENTS_T_HPP

#include <atomic>
#include <future>
#include <optional>
#include <vector>

#include "score/socom/client_connector.hpp"
#include "score/socom/connector_factory.hpp"
#include "score/socom/server_t.hpp"
#include "score/socom/socom_mocks.hpp"
#include "score/socom/temporary_event_subscription.hpp"
#include "score/socom/utilities.hpp"

namespace score::socom
{

/// \brief Facade for the client connector and callback mocks.
///
/// It allows easy configuration of mocks and blocks its destruction until all
/// expectations have been fulfilled. The stored client connector is connected
/// and ready for communication after construction by default.
struct Client_data
{
  private:
    std::atomic<bool> m_event_callback_called{true};
    std::atomic<bool> m_event_request_callback_called{true};
    std::atomic<bool> m_event_subscription_status_change_called{true};
    std::atomic<bool> m_method_callback_called{true};
    std::atomic<bool> m_not_available{true};
    std::atomic<bool> m_available{true};
    std::atomic<bool> m_event_payload_allocate_called{true};
    std::atomic<uint32_t> m_num_event_callback_called{0};
    std::atomic<uint32_t> m_num_method_callback_called{0};
    Client_connector_callbacks_mock m_callbacks;
    Method_reply_callback_mock m_method_callback;
    std::vector<Method_invocation::Uptr> m_method_invocations;
    Client_connector::Uptr m_connector;

    std::atomic<bool>& get_atomic(const Service_state& state);

  public:
    using Vector = std::vector<std::unique_ptr<Client_data>>;

    /// \brief Enum to signal constructor not to connect to the server
    enum No_connect_helper
    {
        no_connect,
        might_connect
    };

    /// \brief Create Client_data and connect to the server
    ///
    /// \param[in] factory factory to create client connector with
    explicit Client_data(Connector_factory& factory);

    /// \brief Create Client_data but do not connect to the server
    ///
    /// \param[in] factory factory to create client connector with
    Client_data(Connector_factory& factory,
                const No_connect_helper& connect_helper,
                Service_state_change_callback state_change_callback = {});

    /// \brief Create Client_data but do not connect to the server
    ///
    /// \param[in] factory factory to create client connector with
    /// \param[in] configuration use this instead of the one stored in factory
    /// \param[in] instance use this instead of the one stored in factory
    Client_data(Connector_factory& factory,
                const No_connect_helper& connect_helper,
                const Service_interface_definition& configuration,
                const Service_instance& instance,
                Service_state_change_callback state_change_callback = {});

    /// \brief Create Client_data with custom configuration, POSIX credentials and connect to the
    /// server
    ///
    /// \param[in] factory factory to create client connector with
    /// \param[in] configuration use this instead of the one stored in factory
    /// \param[in] instance use this instead of the one stored in factory
    /// \param[in] credentials POSIX credentials
    Client_data(Connector_factory& factory,
                const Service_interface_definition& configuration,
                const Service_instance& instance,
                const std::optional<Posix_credentials>& credentials = {});

    Client_data(const Client_data&) = delete;
    Client_data(Client_data&&) = delete;

    /// \brief Block until all expectations have been fulfilled
    ~Client_data();

    Client_data& operator=(const Client_data&) = delete;
    Client_data& operator=(Client_data&&) = delete;

    /// \brief Call subscribe_event() with Event_mode::update
    ///
    /// \param[in] event_id event to subscribe to
    void subscribe_event(const Event_id& event_id, const Event_mode mode = Event_mode::update);

    /// \brief Call unsubscribe_event()
    ///
    /// \param[in] event_id event to unsubscribe from
    void unsubscribe_event(const Event_id& event_id);

    /// \brief Create event subscription for event_id with Event_mode::update
    ///
    /// \param[in] event_id event to subscribe to
    /// \return RAII object holding the subscription
    std::unique_ptr<Temporary_event_subscription> create_event_subscription(const Event_id& event_id);

    /// \brief Create event subscription for event_id with Event_mode::update_and_initial
    ///        but the server is not answering the update event request
    ///
    /// \param[in] server server receiving the event update request
    /// \param[in] event_id event to subscribe to
    /// \param[in] brokenness flag to indicate not to send event update
    /// \return RAII object holding the subscription
    std::unique_ptr<Temporary_event_subscription> create_event_subscription(
        Server_data& server,
        const Event_id& event_id,
        const Temporary_event_subscription::Brokenness& brokenness);

    /// \brief Request event update for event_id
    ///
    /// \param[in] event_id event to request update for
    void request_event_update(const Event_id& event_id) const;

    /// \brief Allocate a payload for the given method ID.
    ///
    /// \param method_id ID of the method for which a payload should be allocated.
    /// \return A writable payload in case of successful operation, otherwise an error.
    score::Result<Writable_payload> allocate_method_call_payload(Method_id method_id);

    /// \brief Call method method_id with payload
    ///
    /// \param[in] method_id method to call
    /// \param[in] payload payload of method
    void call_method(const Method_id& method_id, const Payload& payload);

    /// \brief Call method method_id with payload and reply callback
    ///
    /// \param[in] method_id method to call
    /// \param[in] payload payload of method
    /// \param[in] reply reply callback
    void call_method(const Method_id& method_id, const Payload& payload, Method_reply_callback reply);

    /// \brief Call method method_id with payload and reply callback
    ///
    /// \param[in] method_id method to call
    /// \param[in] payload payload of method
    /// \param[in] reply reply callback
    void call_method(const Method_id& method_id, const Payload& payload, Method_call_reply_data reply);

    /// \brief Call method without callback and without expecting a response
    ///
    /// \param[in] method_id method to call
    /// \param[in] payload the data of the method call
    void call_method_fire_and_forget(const Method_id& method_id, const Payload& payload);

    /// Call method without callback and without expecting a response
    ///
    /// \param[in] method_id method to call
    /// \param[in] payload the data of the method call
    /// \return Method_invocation on success, else error
    score::Result<Method_invocation::Uptr> call_method_fire_and_forget_and_return_invocation(const Method_id& method_id,
                                                                                             const Payload& payload);

    /// \brief Expect state change of configured service
    ///
    /// \param[in] state state into which the service switches to
    /// \return boolean reference which becomes true when the callback is called
    const std::atomic<bool>& expect_service_state_change(const Service_state& state);

    /// \brief Expect state changes of configured service
    ///
    /// \param[in] count number of times the service switches into this state
    /// \param[in] state state into which the service switches to
    /// \return boolean reference which becomes true when the callback is called
    const std::atomic<bool>& expect_service_state_change(size_t count, const Service_state& state);

    /// \brief Expect state changes of configured service
    ///
    /// \param[in] count number of times the service switches into this state
    /// \param[in] state state into which the service switches to
    /// \param[in] conf server configuration received via callback
    /// \return boolean reference which becomes true when the callback is called
    const std::atomic<bool>& expect_service_state_change(
        size_t count,
        const Service_state& state,
        const Optional_reference<const Server_service_interface_definition>& conf);

    /// \brief Expect event payload allocation
    ///
    /// \param[in] event_id event which is updated
    /// \param[in] result the return value of the callback
    /// \return boolean reference which becomes true after the callback has been called
    const std::atomic<bool>& expect_event_payload_allocation(const Event_id& event_id,
                                                             score::Result<Writable_payload> result);

    /// \brief Expect event update
    ///
    /// \param[in] event_id event which is updated
    /// \param[in] payload the data of the send
    /// \return boolean reference which becomes true after the event has been received
    const std::atomic<bool>& expect_event_update(const Event_id& event_id, const Payload& payload);

    /// \brief Expect event updates
    ///
    /// \param[in] count number of times the service switches into this state
    /// \param[in] event_id event which is updated
    /// \param[in] payload the data of the send
    /// \return boolean reference which becomes true after all events have been received
    const std::atomic<bool>& expect_event_updates(const size_t& count,
                                                  const Event_id& event_id,
                                                  const Payload& payload);

    /// Expect event updates
    ///
    /// \param[in] count minimum number of times the service switches into this state
    /// \param[in] event_id event which is updated
    /// \param[in] payload the data of the send
    /// \return future which unblocks after minimum received event updates.
    std::future<void> expect_event_updates_min_number(const std::size_t& count,
                                                      const Event_id& event_id,
                                                      const Payload& payload);

    /// \brief Expect requested event update
    ///
    /// \param[in] event_id event for which an update is requested
    /// \param[in] payload the data of the send
    /// \return boolean reference which becomes true after the event has been received
    const std::atomic<bool>& expect_requested_event_update(const Event_id& event_id, const Payload& payload);

    /// Expect requested event updates
    ///
    /// \param[in] count minimum number of times the service switches into this state
    /// \param[in] event_id event which is updated
    /// \param[in] payload the data of the send
    /// \return future which unblocks after minimum received event updates.
    std::future<void> expect_requested_event_updates_min_number(const std::size_t& count,
                                                                const Event_id& event_id,
                                                                const Payload& payload);

    /// \brief Expect and request event update
    ///
    /// \param[in] event_id event for which an update is requested
    /// \param[in] payload the data of the send
    /// \return boolean reference which becomes true after the event has been received
    const std::atomic<bool>& expect_and_request_event_update(const Event_id& event_id, const Payload& payload);

    /// \brief Expect response and call method
    ///
    /// \param[in] method_id method to call
    /// \param[in] payload the data of the method call
    /// \param[in] method_result response of the method
    /// \return boolean reference which becomes true when the response is received
    const std::atomic<bool>& expect_and_call_method(const Method_id& method_id,
                                                    const Payload& payload,
                                                    const Method_result& method_result);

    /// \brief Expect responses and call methods
    ///
    /// \param[in] count number of times the service switches into this state
    /// \param[in] method_id method to call
    /// \param[in] payload the data of the method call
    /// \param[in] method_result response of the method
    /// \return boolean reference which becomes true after all responses have been received
    const std::atomic<bool>& expect_and_call_methods(const size_t& count,
                                                     const Method_id& method_id,
                                                     const Payload& payload,
                                                     const Method_result& method_result);

    /// \brief Create and connect clients
    ///
    /// \param[in] factory factory used to create and configure the clients
    /// \param[in] size number of clients to create
    /// \return created and connected clients
    static Vector create_clients(Connector_factory& factory, const size_t& size);

    /// \brief Create clients without server connection
    ///
    /// \param[in] factory factory used to create and configure the clients
    /// \param[in] size number of clients to create
    /// \param[in] no_connect do not connect clients to Server_connector
    /// \return created and connected clients
    static Vector create_clients(Connector_factory& factory, const size_t& size, const No_connect_helper& no_connect);

    /// \brief Create and connect clients with custom configuration
    ///
    /// \param[in] factory factory used to create the clients
    /// \param[in] size number of clients to create
    /// \param[in] configuration use this instead of the one stored in factory
    /// \param[in] instance use this instead of the one stored in factory
    /// \return created and connected clients
    static Vector create_clients(Connector_factory& factory,
                                 const size_t& size,
                                 const Service_interface_definition& configuration,
                                 const Service_instance& instance);

    /// \brief Expect event update
    ///
    /// \param[in] clients clients which are subscribed to the event
    /// \param[in] event_id event which is updated
    /// \param[in] payload the data of the send
    /// \return boolean references for each client which become true after the event has been
    /// received
    static Callbacks_called_t expect_event_update(Vector& clients, Event_id event_id, const Payload& payload);

    /// \brief Expect and request event update
    ///
    /// \param[in] clients clients which are subscribed to the event
    /// \param[in] event_id event which is requested to update
    /// \param[in] payload the data of the send
    /// \return boolean references for each client which become true after the event has been
    /// received
    static Callbacks_called_t expect_and_request_event_update(Vector& clients,
                                                              Event_id event_id,
                                                              const Payload& payload);

    /// \brief Expect response and call method
    ///
    /// \param[in] clients clients which are subscribed to the event
    /// \param[in] method_id method to call
    /// \param[in] payload the data of the method call
    /// \param[in] method_result response of the method
    /// \return boolean references for each client which become true after the response has been
    /// received
    static Callbacks_called_t expect_and_call_method(Vector& clients,
                                                     Method_id method_id,
                                                     const Payload& payload,
                                                     const Method_result& method_result);

    /// \brief Create event subscriptions for event_id with Event_mode::update
    ///
    /// \param[in] clients clients to subscribe to event_id
    /// \param[in] event_id event to subscribe to
    /// \return subscription RAII objects for all clients
    static Subscriptions subscribe(const Client_data::Vector& clients, const Event_id& event_id);

    /// \brief Get peer credentials from server.
    ///
    /// \return Result with valid peer credentials or error.
    score::Result<Posix_credentials> get_peer_credentials() const;
};

}  // namespace score::socom

#endif
