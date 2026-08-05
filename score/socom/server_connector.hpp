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

#ifndef SCORE_SOCOM_SERVER_CONNECTOR_HPP
#define SCORE_SOCOM_SERVER_CONNECTOR_HPP

#include <memory>
#include <score/move_only_function.hpp>

#include "score/socom/error.hpp"
#include "score/socom/event.hpp"
#include "score/socom/method.hpp"
#include "score/socom/payload.hpp"
#include "score/socom/posix_credentials.hpp"
#include "score/socom/service_interface_definition.hpp"

namespace score::socom {

class Disabled_server_connector;
class Enabled_server_connector;

/// \brief Function type for indicating an event subscription state change to the service provider.
using Event_subscription_change_callback =
    score::cpp::move_only_function<void(Enabled_server_connector&, Event_id, Event_state)>;

/// \brief Function type for indicating an event update request to the service provider.
using Event_request_update_callback =
    score::cpp::move_only_function<void(Enabled_server_connector&, Event_id)>;

/// \brief Function type for processing any client side method invocation.
using Method_call_credentials_callback = score::cpp::move_only_function<Method_invocation::Uptr(
    Enabled_server_connector&, Method_id, Payload, Method_call_reply_data_opt,
    Posix_credentials const&)>;

/// \brief Function type for indicating a method call payload request to the service provider.
using Method_call_payload_allocate_callback =
    score::cpp::move_only_function<score::Result<Writable_payload>(Enabled_server_connector&,
                                                                   Method_id)>;

class Configuration_getter {
   public:
    virtual ~Configuration_getter() = default;

    [[nodiscard]]
    virtual Server_service_interface_definition const& get_configuration() const noexcept = 0;
    [[nodiscard]]
    virtual Service_instance const& get_service_instance() const noexcept = 0;
};

/// \brief Interface for applications to use a service (server-role).
/// \details This interface represents a Server_connector not visible to any Client_connector(s).
/// After destruction no registered callbacks are called anymore.
/// All user callbacks must not block and shall return quickly (simple algorithms only).
class Disabled_server_connector : public Configuration_getter {
   public:
    /// \brief Alias for an unique pointer to this interface.
    using Uptr = std::unique_ptr<Disabled_server_connector>;

    Disabled_server_connector() = default;
    virtual ~Disabled_server_connector() noexcept = default;

    Disabled_server_connector(Disabled_server_connector const&) = delete;
    Disabled_server_connector(Disabled_server_connector&&) = delete;

    Disabled_server_connector& operator=(Disabled_server_connector const&) = delete;
    Disabled_server_connector& operator=(Disabled_server_connector&&) = delete;

    /// \brief Server_Connector callback interface needed at Server_connector construction, see
    /// Runtime::make_server_connector().
    ///
    /// \details All user callbacks must not block and shall return quickly (simple algorithms
    /// only). No callback is allowed to destroy the Server_connector, otherwise it will result in a
    /// deadlock. If a deadlock situation is detected, a warning will be logged and the application
    /// terminated.
    struct Callbacks {
        /// \brief Callback is called on any client side method invocation.
        Method_call_credentials_callback on_method_call;

        /// \brief Callback is called if an event is subscribed by the first Client_connector or
        /// unsubscribed by the last Client_connector.
        Event_subscription_change_callback on_event_subscription_change;

        /// \brief Callback is called if an event update is requested by any Client_connector.
        /// \details On a call to callback on_event_update_request(), the Server application calls
        /// update_requested_event() or update_event() for the requested event as follows:
        ///   - update_requested_event() is called if no new data is available from the
        /// application (indicate current state only to requesting clients);
        ///   - update_event() is called if new data is available from the application (indicate new
        /// state to all clients).
        Event_request_update_callback on_event_update_request;

        /// \brief Callback is called to allocate method call payloads.
        Method_call_payload_allocate_callback on_method_call_payload_allocate;
    };

    /// \brief Makes the service available to clients.
    /// \details Changes the connector to state 'Enabled' and converts it to an
    /// Enabled_server_connector. Registers the Enabled_server_connector at the SOCom service
    /// registry, connects each matching registered Client_connector to this instance and calls the
    /// callback on_service_state_change(Service_state::available, server_configuration) of each
    /// connected Client_connector instance.
    ///
    /// Server_connector instance callbacks may be called after entering enable().
    /// \param connector Disabled server connector.
    /// \return An enabled server connector.
    [[nodiscard]]
    static std::unique_ptr<Enabled_server_connector> enable(
        std::unique_ptr<Disabled_server_connector> connector);

   protected:
    /// \cond INTERNAL
    virtual Enabled_server_connector* enable() = 0;
    /// \endcond
};

/// \brief Interface for applications to use a service (server-role).
/// \details This interface represents an enabled Server_connector, thus it is registered by the
/// SOCom service registry and available to connected Client_connector(s).
///
/// If a client calls Client_connector::call_method, then the callback on_method_called() is called.
///
/// If the client-aggregated need of an event changes, then callback on_event_subscription_change()
/// is called.
///
/// If a client requests the current value of an event, then callback on_event_update_request() is
/// called.
///
/// If the passed parameter server_id  is not valid (not contained in
/// Server_service_interface_definition), service API calls have no effect and return
/// Server_connector_error::logic_error_id_out_of_range.
class Enabled_server_connector : public Configuration_getter {
   public:
    /// \brief Alias for an unique pointer to this interface.
    using Uptr = std::unique_ptr<Enabled_server_connector>;

    /// \brief Constructor.
    Enabled_server_connector() = default;

    /// \brief Destructor.
    /// \details Disconnects from Client_connectors and destroys the Server_connector. After
    /// destruction no registered callbacks are called anymore.
    ///
    /// Implicitly calls disable() and deallocates the instance resources.
    ///
    /// Detects deadlocks which are caused by destroying the Client_connector from a running
    /// Client_connector callback. When a deadlock is detected, the destructor shall log and
    /// terminate the application.
    virtual ~Enabled_server_connector() noexcept = default;

    Enabled_server_connector(Enabled_server_connector const&) = delete;
    Enabled_server_connector(Enabled_server_connector&&) = delete;

    Enabled_server_connector& operator=(Enabled_server_connector const&) = delete;
    Enabled_server_connector& operator=(Enabled_server_connector&&) = delete;

    /// \brief Removes the connection to the clients.
    /// \details Calls the callback on_service_state_change(Service_state::not_available) of
    /// each connected Client_connector instances. It disconnects from all connected
    /// Client_connector instances and blocks until all clients are disconnected.
    /// \param connector Enabled server connector.
    /// \return A disabled server connector.
    [[nodiscard]]
    static std::unique_ptr<Disabled_server_connector> disable(
        std::unique_ptr<Enabled_server_connector> connector) noexcept;

    /// \brief Allocates a payload for the given event ID.
    ///
    /// This requires a Client_connector to be subscribed to the event to which payload allocation
    /// is delegated.
    ///
    /// \param event_id ID of the event for which a payload should be allocated.
    /// \return A writable payload in case of successful operation, otherwise an error.
    [[nodiscard]]
    virtual Result<Writable_payload> allocate_event_payload(Event_id event_id) noexcept = 0;

    /// \brief Distributes new event data to all subscribed Client_connectors.
    /// \details Clears the list of event update requesters for the event server_id.
    ///
    /// Calls the callback on_event_update(client_id, payload) for each connected Client_connector
    /// which is subscribed to event server_id.
    /// \param server_id ID of the event.
    /// \param payload Event data.
    /// \return Void in case of successful operation, otherwise an error.
    virtual Result<void> update_event(Event_id server_id, Payload payload) noexcept = 0;

    /// \brief Distributes new event data to all event update requesting Client_connectors.
    /// \details Clears the list of event update requesters for the event server_id.
    ///
    /// Calls the callback on_event_requested_update(client_id, payload) for each connected
    /// Client_connector instance in a list of update requesters for event server_id.
    /// \param server_id ID of the event.
    /// \param payload Event data.
    /// \return Void in case of successful operation, otherwise an error.
    virtual Result<void> update_requested_event(Event_id server_id, Payload payload) noexcept = 0;

    /// \brief Retrieves the mode of the event server_id.
    /// \details Returns the combined event subscription mode for event server_id, see
    /// Client_connector::subscribe_event().
    ///
    /// Returns Event_mode::update_and_initial_value if any client has subscribed with
    /// Event_mode::update_and_initial_value.
    ///
    /// Returns Event_mode::update if no Client_connector instance has subscribed to this event yet.
    /// \param server_id ID of the event.
    /// \return An event mode in case of successful operation, otherwise an error.
    [[nodiscard]] virtual Result<Event_mode> get_event_mode(Event_id server_id) const noexcept = 0;

   protected:
    /// \cond INTERNAL
    virtual Disabled_server_connector* disable() noexcept = 0;
    /// \endcond
};

}  // namespace score::socom

#endif  // SCORE_SOCOM_SERVER_CONNECTOR_HPP
