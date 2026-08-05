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

#ifndef SCORE_SOCOM_RUNTIME_HPP
#define SCORE_SOCOM_RUNTIME_HPP

#include <functional>
#include <memory>

#include "score/socom/client_connector.hpp"
#include "score/socom/error.hpp"
#include "score/socom/posix_credentials.hpp"
#include "score/socom/server_connector.hpp"
#include "score/socom/service_interface_identifier.hpp"

namespace score::socom {

/// \brief Service bridge identification.
class Bridge_identity {
   public:
    /// \brief Creates instance of Bridge_identity.
    /// \param instance Reference instance.
    /// \tparam T Type of instance.
    /// \return Bridge_identity object.
    template <typename T>
    static Bridge_identity make(T const& instance) {
        return Bridge_identity{static_cast<void const*>(&instance)};
    }

    /// \brief Operator == for Bridge_identity.
    /// \param lhs Bridge_identity to compare.
    /// \param rhs Bridge_identity to compare.
    /// \return True in case of equality, otherwise false.
    friend bool operator==(Bridge_identity lhs, Bridge_identity rhs) noexcept {
        return lhs.m_identity == rhs.m_identity;
    }

   private:
    explicit Bridge_identity(void const* identity) : m_identity{identity} {}

    void const* m_identity;
};

/// \brief Operator != for Bridge_identity.
/// \param lhs Left-hand side of operator.
/// \param rhs Right-hand side of operator.
/// \return True in case of inequality, otherwise false.
inline bool operator!=(Bridge_identity const& lhs, Bridge_identity const& rhs) {
    return !(lhs == rhs);
}

/// \brief Interface class for Service_bridge_registration RAII type (see Runtime).
class Service_bridge_registration_handle {
   public:
    Service_bridge_registration_handle() = default;
    virtual ~Service_bridge_registration_handle() = default;

    Service_bridge_registration_handle(Service_bridge_registration_handle const&) = delete;
    Service_bridge_registration_handle(Service_bridge_registration_handle&&) = delete;

    Service_bridge_registration_handle& operator=(Service_bridge_registration_handle const&) =
        delete;
    Service_bridge_registration_handle& operator=(Service_bridge_registration_handle&&) = delete;

    /// \brief Getter for Bridge_identity.
    /// \return Bridge_identity object.
    [[nodiscard]] virtual Bridge_identity get_identity() const = 0;
};

/// \brief Interface class for Service_request RAII type (see Runtime).
class Service_request_handle {
   public:
    Service_request_handle() = default;
    virtual ~Service_request_handle() = default;

    Service_request_handle(Service_request_handle const&) = delete;
    Service_request_handle(Service_request_handle&&) = delete;

    Service_request_handle& operator=(Service_request_handle const&) = delete;
    Service_request_handle& operator=(Service_request_handle&&) = delete;
};

/// \brief RAII object that represents a service bridge registration at the runtime, see
/// Runtime::register_service_bridge()
using Service_bridge_registration = std::unique_ptr<Service_bridge_registration_handle>;
/// \brief RAII object that represents an service request from the runtime to a service bridge, see
/// Runtime::register_service_bridge().
using Service_request = std::unique_ptr<Service_request_handle>;

/// \brief Request_service interface type signature.
using Request_service_function =
    std::function<Service_request(Service_interface_definition const&, Service_instance const&)>;

/// \brief Interface that provides access to the service oriented communication (SOCom) middleware.
/// \details SOCom implements a client-service-server based architectural pattern.
/// A service is an instance (Service_instance) of an interface (Service_interface).
/// A server provides a service.
/// Clients use services.
/// The service pattern makes client and server independent from concrete instances of each other
/// (loose coupling). Depending on their availability, SOCom performs the dependency resolution
/// client/server connection and disconnection at runtime.

/// A service interface supports the following communication patterns:
///   - method call (1:1)
///     - client-server-client
///     - client-server
///   - event, also known as publish/subscribe (1:n)
///     - server-clients
class Runtime {
   public:
    /// \brief Alias for an unique pointer to this interface.
    using Uptr = std::unique_ptr<Runtime>;

    Runtime() = default;
    virtual ~Runtime() noexcept = default;
    Runtime(Runtime const&) = delete;
    Runtime(Runtime&&) = delete;
    Runtime& operator=(Runtime const&) = delete;
    Runtime& operator=(Runtime&&) = delete;

    /// \brief Creates a new client connector.
    /// \details Returns a new instance Client_connector registered as a service user for the
    /// service defined by the configuration.interface and instance parameters to the SOCom service
    /// registry.
    ///
    /// If the first client connector for [configuration, instance] is created and the requested
    /// service is locally not present, make_client_connector() calls
    /// request_service(configuration, instance) on every bridge (already registered or registered
    /// later) and stores the Service_request RAII objects.
    ///
    /// If the last client connector for [configuration, instance] is destroyed,
    /// make_client_connector() deletes all associated service bridge Service_request RAII objects.
    /// \param configuration Service interface configuration.
    /// \param instance Service instance.
    /// \param callbacks User callbacks to be called based on the internal states.
    /// \return A pointer to a Client_connector instance in case of successful operation, otherwise
    /// an error.
    /// \note Construction_error::callback_missing is returned if any of the callbacks is not set.
    /// \note This method sets the values returned from getuid() and getpid() (unistd.h) as
    /// credentials of the returned Client_connector.
    [[nodiscard]]
    virtual Result<Client_connector::Uptr> make_client_connector(
        Service_interface_definition configuration, Service_instance instance,
        Client_connector::Callbacks callbacks) noexcept = 0;

    /// \brief Creates a new client connector.
    /// \details This method behaves the same as the make_client_connector() above.
    /// Additionally, custom posix credentials can be passed.
    /// \param configuration Service interface configuration.
    /// \param instance Service instance.
    /// \param callbacks User callbacks to be called based on the internal states.
    /// \param credentials Posix credentials to be set for the client connector.
    /// \return A pointer to a Client_connector instance in case of successful operation, otherwise
    /// an error.
    /// \note Construction_error::callback_missing is returned if any of the callbacks is not set.
    [[nodiscard]]
    virtual Result<Client_connector::Uptr> make_client_connector(
        Service_interface_definition configuration, Service_instance instance,
        Client_connector::Callbacks callbacks, Posix_credentials const& credentials) noexcept = 0;

    /// \brief Creates a new server connector.
    /// \details Returns a new instance of Disabled_server_connector registered as a service
    /// provider for the service defined by the configuration.interface and instance parameters to
    /// the SOCom service registry if this service does not exist in the runtime service registry
    /// yet.
    ///
    /// Logs an error and returns Construction_error::duplicate_service if a service defined by the
    /// configuration.interface and instance parameters is already registered in the runtime service
    /// registry.

    /// Returns Construction_error::callback_missing if any of the callbacks is not set.
    /// \param configuration Service interface configuration.
    /// \param instance Service instance.
    /// \param callbacks User callbacks to be called based on the internal states.
    /// \return A pointer to a server connector instance in case of successful operation, otherwise
    /// an error.
    /// \note This method sets the values returned from getuid() and getpid() (unistd.h) as
    /// credentials of the returned Disabled_server_connector.
    [[nodiscard]]
    virtual Result<Disabled_server_connector::Uptr> make_server_connector(
        Server_service_interface_definition configuration, Service_instance instance,
        Disabled_server_connector::Callbacks callbacks) noexcept = 0;

    /// \brief Creates a new server connector.
    /// \details This method behaves the same as the make_server_connector() above.
    /// Additionally, custom posix credentials can be passed.
    /// \param configuration Service interface configuration.
    /// \param instance Service instance.
    /// \param callbacks User callbacks to be called based on the internal states.
    /// \param credentials Posix credentials to be set for the server connector.
    /// \return A pointer to a server connector instance in case of successful operation, otherwise
    /// an error.
    [[nodiscard]]
    virtual Result<Disabled_server_connector::Uptr> make_server_connector(
        Server_service_interface_definition configuration, Service_instance instance,
        Disabled_server_connector::Callbacks callbacks,
        Posix_credentials const& credentials) noexcept = 0;

    /// \brief Registers a bridge which transports events or method calls over an IPC channel.
    /// \param identity Bridge identity.
    /// \param request_service Function to call if the requested service is not present locally.
    /// \return A registration RAII object in case of successful operation, otherwise an error.
    /// \note Construction_error::callback_missing is returned if any of the callbacks is not set.
    [[nodiscard]]
    virtual Result<Service_bridge_registration> register_service_bridge(
        Bridge_identity identity, Request_service_function request_service) noexcept = 0;
};

/// \brief Function to instantiate a Runtime object.
/// \param logger Logger for logging messages.
/// \return Pointer to Runtime object.
Runtime::Uptr create_runtime();

}  // namespace score::socom

#endif  // SCORE_SOCOM_RUNTIME_HPP
