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

#ifndef SOCOM_CONNECTOR_FACTORY_HPP
#define SOCOM_CONNECTOR_FACTORY_HPP

#include "score/socom/client_connector.hpp"
#include "score/socom/posix_credentials.hpp"
#include "score/socom/runtime.hpp"
#include "score/socom/server_connector.hpp"
#include "score/socom/service_interface_definition.hpp"
#include "score/socom/socom_mocks.hpp"
#include "score/socom/utilities.hpp"

namespace score::socom {

/// \brief Creates client and server connectors with default values
class Connector_factory {
    Runtime::Uptr m_runtime;
    Server_service_interface_definition m_configuration;
    Service_instance m_instance;

    Runtime& get_runtime();

   public:
    /// \brief Creates a Connector_factory with default configuration
    ///
    /// \param[in] configuration default configuration for created server and clients
    /// \param[in] instance default intance for created server and clients
    Connector_factory(Server_service_interface_definition configuration, Service_instance instance);

    /// \brief Creates a Connector_factory with default configuration
    ///
    /// \param[in] sif default service interface for created server and clients
    /// \param[in] version Version of the server interface
    /// \param[in] methods Number of Methods
    /// \param[in] events Number of Events
    /// \param[in] instance default intance for created server and clients
    Connector_factory(Service_interface_identifier const& sif, Num_of_methods num_methods,
                      Num_of_events num_events, Service_instance instance);

    /// \brief Copy constructor which copies the configuraton of con_fac
    ///
    /// \param[in] con_fac objects whichs configuration is copied
    Connector_factory(Connector_factory const& con_fac);

    Connector_factory(Connector_factory&&) = default;

    ~Connector_factory() = default;

    Connector_factory& operator=(Connector_factory const&) = delete;
    Connector_factory& operator=(Connector_factory&&) = delete;

    /// \brief Registers bridge callbacks at the runtime until registration is destroyed
    ///
    /// Bridges can provide additional services via IPC. The callbacks are used the query the
    /// bridges for services.
    ///
    /// \param[in] identity identity of the bridge to register, used for later identification
    /// \param[in] request_service
    /// \return RAII object which keeps the registration alive until it is destroyed
    ::score::Result<Service_bridge_registration> register_service_bridge(
        Bridge_identity identity, Request_service_function request_service);

    /// \brief Create server connector with default configuration
    ///
    /// \param[in] sc_callbacks callbacks for the server connector
    /// \return server connector
    Disabled_server_connector::Uptr create_server_connector(
        Optional_reference<Server_connector_callbacks_mock> sc_callbacks);

    /// \brief Create server connector with default configuration
    ///
    /// \param[in] sc_callbacks callbacks for the server connector
    /// \return server connector factory result
    Disabled_server_connector::Uptr create_server_connector(
        Disabled_server_connector::Callbacks sc_callbacks);

    /// \brief Create server connector with default configuration
    ///
    /// \param[in] sc_callbacks callbacks for the server connector
    /// \return server connector factory result
    ::score::Result<Disabled_server_connector::Uptr> create_server_connector_with_result(
        Disabled_server_connector::Callbacks sc_callbacks);

    /// \brief Create server connector with default configuration
    ///
    /// \param[in] sc_callbacks callbacks for the server connector
    /// \return server connector factory result
    ::score::Result<Disabled_server_connector::Uptr> create_server_connector_with_result(
        Optional_reference<Server_connector_callbacks_mock> sc_callbacks);

    /// \brief Create server connector with custom configuration
    ///
    /// \param[in] configuration other configuration
    /// \param[in] instance other instance
    /// \param[in] sc_callbacks callbacks for the server connector
    /// \return server connector
    Disabled_server_connector::Uptr create_server_connector(
        Server_service_interface_definition const& configuration, Service_instance const& instance,
        Optional_reference<Server_connector_callbacks_mock> sc_callbacks);

    /// \brief Create server connector with custom configuration and POSIX credentials
    ///
    /// \param[in] configuration other configuration
    /// \param[in] instance other instance
    /// \param[in] sc_callbacks callbacks for the server connector
    /// \param[in] credentials POSIX credentials
    /// \return server connector
    Disabled_server_connector::Uptr create_server_connector(
        Server_service_interface_definition const& configuration, Service_instance const& instance,
        Optional_reference<Server_connector_credentials_callbacks_mock> sc_callbacks,
        Posix_credentials const& credentials);

    /// \brief Create and enable server connector with default configuration
    ///
    /// \param[in] sc_callbacks callbacks for the server connector
    /// \return enabled server connector
    Enabled_server_connector::Uptr create_and_enable(
        Optional_reference<Server_connector_callbacks_mock> sc_callbacks);

    /// \brief Create and enable server connector with custom configuration
    ///
    /// \param[in] configuration other configuration
    /// \param[in] instance other instance
    /// \param[in] sc_callbacks callbacks for the server connector
    /// \return enabled server connector
    Enabled_server_connector::Uptr create_and_enable(
        Server_service_interface_definition const& configuration, Service_instance const& instance,
        Optional_reference<Server_connector_callbacks_mock> sc_callbacks);

    /// \brief Create and enable server connector with custom configuration and POSIX credentials
    ///
    /// \param[in] configuration other configuration
    /// \param[in] instance other instance
    /// \param[in] sc_callbacks callbacks for the server connector
    /// \param[in] credentials POSIX credentials
    /// \return enabled server connector
    Enabled_server_connector::Uptr create_and_enable(
        Server_service_interface_definition const& configuration, Service_instance const& instance,
        Optional_reference<Server_connector_credentials_callbacks_mock> sc_callbacks,
        Posix_credentials const& credentials);

    /// \brief Create client connector with default configuration
    ///
    /// \param[in] cc_callbacks callbacks for the client connector
    /// \return client connector
    Client_connector::Uptr create_client_connector(
        Optional_reference<Client_connector_callbacks_mock> cc_callbacks);

    /// \brief Create client connector with default configuration
    ///
    /// \param[in] cc_callbacks callbacks for the client connector
    /// \return client connector
    Client_connector::Uptr create_client_connector(Client_connector::Callbacks cc_callbacks);

    /// \brief Create client connector with custom configuration
    ///
    /// \param[in] configuration other configuration
    /// \param[in] instance other instance
    /// \param[in] cc_callbacks callbacks for the client connector
    /// \return client connector
    Client_connector::Uptr create_client_connector(
        Service_interface_definition const& configuration, Service_instance const& instance,
        Optional_reference<Client_connector_callbacks_mock> cc_callbacks);

    /// \brief Create client connector with custom configuration and POSIX credentials
    ///
    /// \param[in] configuration other configuration
    /// \param[in] instance other instance
    /// \param[in] cc_callbacks callbacks for the client connector
    /// \param[in] credentials POSIX credentials
    /// \return client connector
    Client_connector::Uptr create_client_connector(
        Service_interface_definition const& configuration, Service_instance const& instance,
        Optional_reference<Client_connector_callbacks_mock> cc_callbacks,
        Posix_credentials const& credentials);

    /// \brief Create client connector with custom configuration and POSIX credentials
    ///
    /// \param[in] configuration other configuration
    /// \param[in] instance other instance
    /// \param[in] cc_callbacks callbacks for the client connector
    /// \param[in] credentials POSIX credentials
    /// \return client connector
    ::score::Result<Client_connector::Uptr> create_client_connector_with_result(
        Service_interface_definition const& configuration, Service_instance const& instance,
        Optional_reference<Client_connector_callbacks_mock> cc_callbacks,
        std::optional<Posix_credentials> const& credentials);

    /// \brief Create client connector with custom configuration
    ///
    /// \param[in] configuration other configuration
    /// \param[in] instance other instance
    /// \param[in] cc_callbacks callbacks for the client connector
    /// \return client connector
    ::score::Result<Client_connector::Uptr> create_client_connector_with_result(
        Service_interface_definition const& configuration, Service_instance const& instance,
        Client_connector::Callbacks cc_callbacks);

    /// \brief Create client connector with custom configuration and POSIX credentials
    ///
    /// \param[in] configuration other configuration
    /// \param[in] instance other instance
    /// \param[in] cc_callbacks callbacks for the client connector
    /// \param[in] credentials POSIX credentials
    /// \return client connector
    ::score::Result<Client_connector::Uptr> create_client_connector_with_result(
        Service_interface_definition const& configuration, Service_instance const& instance,
        Client_connector::Callbacks cc_callbacks, Posix_credentials const& credentials);

    /// \brief Create and connect client connector with default configuration
    ///
    /// \param[in] cc_callbacks callbacks for the client connector
    /// \return connected client connector
    Client_connector::Uptr create_and_connect(
        Optional_reference<Client_connector_callbacks_mock> cc_callbacks);

    /// \brief Create and connect client connector with custom configuration
    ///
    /// \param[in] configuration other configuration
    /// \param[in] instance other instance
    /// \param[in] cc_callbacks callbacks for the client connector
    /// \param[in] credentials optional POSIX credentials
    /// \return connected client connector
    Client_connector::Uptr create_and_connect(
        Service_interface_definition const& configuration, Service_instance const& instance,
        Optional_reference<Client_connector_callbacks_mock> cc_callbacks,
        std::optional<Posix_credentials> const& credentials);

    /// \return default configuration for server and client connector
    Server_service_interface_definition const& get_configuration() const;

    /// \return default instance for server and client connector
    Service_instance const& get_instance() const;

    /// \return method names in same order as used by server and client
    std::size_t get_num_methods() const noexcept;

    /// \return event names in same order as used by server and client
    std::size_t get_num_events() const noexcept;
};

}  // namespace score::socom

#endif
