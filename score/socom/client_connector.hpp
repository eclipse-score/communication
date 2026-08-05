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

#ifndef SRC_SOCOM_INCLUDE_SCORE_SOCOM_CLIENT_CONNECTOR
#define SRC_SOCOM_INCLUDE_SCORE_SOCOM_CLIENT_CONNECTOR

#include <memory>
#include <optional>
#include <score/move_only_function.hpp>

#include "score/socom/error.hpp"
#include "score/socom/event.hpp"
#include "score/socom/method.hpp"
#include "score/socom/payload.hpp"
#include "score/socom/posix_credentials.hpp"
#include "score/socom/service_interface_definition.hpp"

namespace score::socom {

class Client_connector;

/// \brief Service states from the service user viewpoint.
enum class Service_state : std::uint8_t {
    /// Service is not available.
    not_available = 0,
    /// Service is available.
    available = 1,
};

/// \brief Function type for indicating service state changes to the service user.
using Service_state_change_callback = score::cpp::move_only_function<void(
    Client_connector const&, Service_state, Server_service_interface_definition const&)>;

/// \brief Function type for indicating event updates to the service user.
using Event_update_callback =
    score::cpp::move_only_function<void(Client_connector const&, Event_id, Payload)>;

/// \brief Function type for allocating event payloads.
using Event_payload_allocate_callback =
    score::cpp::move_only_function<Result<Writable_payload>(Client_connector const&, Event_id)>;

/// \brief Interface for applications to use a service (client-role).
/// \details Changes of service instance state are indicated by callback on_service_state_change.
///
/// A Client_connector instance is connected to a Server_connector instance only if the service
/// interfaces are compatible. The compatibility check contains checks for semantic version and
/// service interface members.
///
/// If the service state is not Service_state::available, service API calls have no effect and
/// return Error::runtime_error_service_not_available.
///
/// If the passed parameter client_id is not valid (not contained in the client connector or server
/// connector specific Service_interface_definition), service API calls have no effect and
/// return Error::logic_error_id_out_of_range.
///
/// If the Service_interface_definition of the Client_connector instance contains a member
/// configuration, then the client_id is translated to the server_id based on the matching member
/// names (configuration.members.events and configuration.members.methods).
///
/// If the Service_interface_definition of the Client_connector instance does not contain a
/// member configuration, then the client_id is translated to the server_id 1:1.
///
/// The Client_connector callback on_event_update is called if an available Enabled_server_connector
/// calls update_event().
///
/// The Client_connector callback on_event_requested_update is called if an available
/// Enabled_server_connector instance calls update_requested_event() and the
/// Client_connector instance previously requested an event update with request_event_update().
class Client_connector {
   public:
    /// \brief Alias for an unique pointer to this interface.
    using Uptr = std::unique_ptr<Client_connector>;

    /// \brief Client_connector callback interface needed at Client_connector construction, see
    /// Runtime::make_client_connector().
    ///
    /// \attention All user callbacks must not block and shall return quickly (simple algorithms
    /// only). No callback is allowed to destroy the Client_connector, otherwise it will result in a
    /// deadlock. If a deadlock situation is detected, a warning will be logged and the application
    /// terminated.
    struct Callbacks {
        /// \brief Callback is called on any service state change.
        Service_state_change_callback on_service_state_change;
        /// \brief Callback is called on a server triggered event update.
        Event_update_callback on_event_update;
        /// \brief Callback is called on a client requested event update, see
        /// Client_connector::subscribe_event() and Client_connector::request_event_update().
        Event_update_callback on_event_requested_update;
        /// \brief Callback is called to allocate event payloads.
        Event_payload_allocate_callback on_event_payload_allocate;
    };

    /// \brief Constructor.
    /// \details A Client_connector instance and a Server_connector instance do not match under the
    /// following conditions:
    ///   - the instance.id of both is different;
    ///   - the configuration.interface.id of both is different;
    ///   - the configuration.interface.version.major of both is different;
    ///   - the Client_connector's configuration.interface.version.minor is larger than the
    /// Server_connector's.
    ///
    /// After construction the service state is always Service_state::not_available.
    /// This initial state is not indicated through the callback on_service_state_change().
    ///
    /// If the SOCom service registry connects a Client_connector to the matching Server_connector,
    /// then the SOCom service registry calls the callback
    /// on_service_state_change(Service_state::available, ...).
    ///
    /// If the SOCom service registry disconnects an available Server_connector from a
    /// Client_connector, then the Client_connector calls the callback
    /// on_service_state_change(Service_state::not_available, ...).
    Client_connector() = default;

    /// \brief Destructor.
    /// \details Unregisters from the server connector and destroys the client connector.
    /// After destruction no registered callbacks are called any more.
    ///
    /// Blocks until all operations have completed and the service state is (implicitly)
    /// Service_state::not_available. It does not call callback on_service_state_change().
    ///
    /// Aborts all method calls. This ensures no method reply will be invoked after completion the
    /// destructor.
    ///
    /// Implicitly unsubscribes all subscribed events.
    ///
    /// Detect deadlocks, which are caused by destroying the Client_connector from a running
    /// Client_connector callback. When a deadlock is detected, the destructor logs and
    /// terminates the application.
    virtual ~Client_connector() noexcept = default;

    Client_connector(Client_connector const&) = delete;
    Client_connector(Client_connector&&) = delete;

    Client_connector& operator=(Client_connector const&) = delete;
    Client_connector& operator=(Client_connector&&) = delete;

    /// \brief Allocate a payload for the given method ID.
    ///
    /// This requires a Server_connector to be connected to which payload allocation is delegated.
    ///
    /// \param method_id ID of the method for which a payload should be allocated.
    /// \return A writable payload in case of successful operation, otherwise an error.
    [[nodiscard]]
    virtual Result<Writable_payload> allocate_method_call_payload(Method_id method_id) noexcept = 0;

    /// \brief Subscribe an event to receive event updates from the Server_connector.
    /// \details The mode value Event_mode::update_and_initial_value supports the field use-case.
    ///
    /// The user is responsible for calling subscribe_event() again, if the service state
    /// transitions to Service_state::available and a subscription is required.
    ///
    /// If the service state is Service_state::available, then the Enabled_server_connector instance
    /// registers this Client_connector as subscribed for event server_id.
    ///
    /// The available Enabled_server_connector instance combines the mode parameter with modes of
    /// other clients subscription's and stores the result.
    ///
    /// The mode value Event_mode::update_and_initial_value is dominant while mode value
    /// Event_mode::update is recessive.
    ///
    /// If one subscription requests mode value Event_mode::update_and_initial_value,
    /// then the resulting stored mode value is Event_mode::update_and_initial_value.
    ///
    /// All subscriptions are lost if the service state Service_state::available is left.
    ///
    /// If this is the first subscription for this event server_id at the Enabled_server_connector
    /// instance (no matter from which Client_connector), then the Enabled_server_connector instance
    /// calls callback on_event_subscription_change(server_id, Event_state::subscribed).
    ///
    /// If this is the first subscription for this event server_id at the Enabled_server_connector
    /// instance and the parameter mode is Event_mode::update_and_initial_value, then the
    /// Enabled_server_connector instance stores the Client_connector instance in a list of update
    /// requesters for event server_id and calls callback on_event_update_request(server_id) after
    /// calling callback on_event_subscription_change().
    ///
    /// \param client_id ID of the event.
    /// \param mode Mode of the event.
    /// \return Void in case of successful operation, otherwise an error.
    virtual Result<Blank> subscribe_event(Event_id client_id, Event_mode mode) const noexcept = 0;

    /// \brief Unsubscribes from an event to stop receiving event updates.
    /// \details If the service state is Service_state::available, then the available
    /// Enabled_server_connector instance unregisters this Client_connector for event server_id and
    /// removes this Client_connector instance from the list of update requesters for event
    /// server_id.
    ///
    /// If this is the last Client_connector instance unsubscribing for a specific event server_id
    /// at the Enabled_server_connector instance, then the Enabled_server_connector instance calls
    /// callback on_event_subscription_change(server_id, Event_state::not_subscribed).
    /// \param client_id ID of the event.
    /// \return Void in case of successful operation, otherwise an error.
    virtual Result<Blank> unsubscribe_event(Event_id client_id) const noexcept = 0;

    /// \brief Requests an event update.
    /// \details If the service state is Service_state::available, then the available
    /// Enabled_server_connector instance stores the Client_connector instance in a list of update
    /// requesters for event server_id and calls callback on_event_update_request(server_id) if this
    /// is the first update_request for the event.
    /// \param client_id ID of the event.
    /// \return Void in case of successful operation, otherwise an error.
    virtual Result<Blank> request_event_update(Event_id client_id) const noexcept = 0;

    /// \brief Calls a method at the Server_connector side.
    /// \details If reply_data is nullopt, then the Server application (of the
    /// Enabled_server_connector instance) and the Method_invocation object returned do not allocate
    /// any resources for this method call and callback on_method_reply() will not be called.
    ///
    /// If reply_data is not nullopt, then the server application (of the
    /// Enabled_server_connector instance) returns a Method_invocation object which allocates
    /// resources required for the ongoing method invocation. Once the method invocation is
    /// completed, the server application calls reply_data.reply_callback().
    /// Discarding the Method_invocation object cancels the method invocation.
    ///
    /// If the service state is Service_state::available, then the available Server_connector
    /// instance calls the callback on_method_call(server_id, payload, reply_data).
    /// \param client_id ID of the method.
    /// \param payload Payload to be called with.
    /// \param reply_data Callback and payload buffer in case a reply is requested.
    /// \return A pointer to a Method_invocation object in case of successful invocation, otherwise
    /// an error.
    [[nodiscard]] virtual Result<Method_invocation::Uptr> call_method(
        Method_id client_id, Payload payload,
        Method_call_reply_data_opt reply_data = std::nullopt) const noexcept = 0;

    /// \brief Retrieves the peer posix credentials from the server.
    /// \details If the client connector is not connected, then an error is returned.
    /// \return Posix credentials in case of successful operation, otherwise an error.
    [[nodiscard]] virtual Result<Posix_credentials> get_peer_credentials() const noexcept = 0;

    [[nodiscard]] virtual Service_interface_definition const& get_configuration()
        const noexcept = 0;
    [[nodiscard]] virtual Service_instance const& get_service_instance() const noexcept = 0;
    [[nodiscard]] virtual bool is_service_available() const noexcept = 0;
};

}  // namespace score::socom

#endif  // SRC_SOCOM_INCLUDE_SCORE_SOCOM_CLIENT_CONNECTOR
