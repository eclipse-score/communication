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

#include "score/socom/connector_factory.hpp"

#include <optional>

#include "gtest/gtest.h"
#include "score/socom/utilities.hpp"

using ::testing::_;
using ::testing::Assign;
using ::testing::DoAll;

namespace score::socom {

Connector_factory::Connector_factory(Server_service_interface_definition configuration,
                                     Service_instance instance)
    : m_runtime{create_runtime()},
      m_configuration{std::move(configuration)},
      m_instance{instance} {}

Connector_factory::Connector_factory(Service_interface_identifier const& sif,
                                     Num_of_methods const num_methods,
                                     Num_of_events const num_events, Service_instance instance)
    : Connector_factory{Server_service_interface_definition{sif, num_methods, num_events},
                        instance} {}

Connector_factory::Connector_factory(Connector_factory const& con_fac)
    : m_runtime{create_runtime()},
      m_configuration{con_fac.get_configuration()},
      m_instance{con_fac.get_instance()} {}

Runtime& Connector_factory::get_runtime() { return *m_runtime; }

score::Result<Service_bridge_registration> Connector_factory::register_service_bridge(
    Bridge_identity identity, Request_service_function request_service) {
    return m_runtime->register_service_bridge(identity, std::move(request_service));
}

Disabled_server_connector::Uptr Connector_factory::create_server_connector(
    Optional_reference<Server_connector_callbacks_mock> sc_callbacks) {
    return create_server_connector(m_configuration, m_instance, std::move(sc_callbacks));
}

Disabled_server_connector::Uptr Connector_factory::create_server_connector(
    Disabled_server_connector::Callbacks sc_callbacks) {
    auto sc = create_server_connector_with_result(std::move(sc_callbacks));
    EXPECT_TRUE(sc);
    return std::move(sc).value();
}

score::Result<Disabled_server_connector::Uptr>
Connector_factory::create_server_connector_with_result(
    Disabled_server_connector::Callbacks sc_callbacks) {
    return get_runtime().make_server_connector(m_configuration, m_instance,
                                               std::move(sc_callbacks));
}

score::Result<Disabled_server_connector::Uptr>
Connector_factory::create_server_connector_with_result(
    Optional_reference<Server_connector_callbacks_mock> sc_callbacks) {
    if (sc_callbacks) {
        return create_server_connector_with_result(create_server_callbacks(*sc_callbacks));
    }

    return create_server_connector_with_result(Disabled_server_connector::Callbacks{});
}

Disabled_server_connector::Uptr Connector_factory::create_server_connector(
    Server_service_interface_definition const& configuration, Service_instance const& instance,
    Optional_reference<Server_connector_callbacks_mock> sc_callbacks) {
    auto callbacks = sc_callbacks ? create_server_callbacks(*sc_callbacks)
                                  : Disabled_server_connector::Callbacks{};
    auto sc = get_runtime().make_server_connector(configuration, instance, std::move(callbacks));
    EXPECT_TRUE(sc);
    return std::move(sc).value();
}

Disabled_server_connector::Uptr Connector_factory::create_server_connector(
    Server_service_interface_definition const& configuration, Service_instance const& instance,
    Optional_reference<Server_connector_credentials_callbacks_mock> sc_callbacks,
    Posix_credentials const& credentials) {
    auto callbacks = sc_callbacks ? create_server_callbacks(*sc_callbacks)
                                  : Disabled_server_connector::Callbacks{};
    auto sc = get_runtime().make_server_connector(configuration, instance, std::move(callbacks),
                                                  credentials);
    EXPECT_TRUE(sc);
    return std::move(sc).value();
}

Enabled_server_connector::Uptr Connector_factory::create_and_enable(
    Optional_reference<Server_connector_callbacks_mock> sc_callbacks) {
    return create_and_enable(m_configuration, m_instance, std::move(sc_callbacks));
}

Enabled_server_connector::Uptr Connector_factory::create_and_enable(
    Server_service_interface_definition const& configuration, Service_instance const& instance,
    Optional_reference<Server_connector_callbacks_mock> sc_callbacks) {
    return Disabled_server_connector::enable(
        create_server_connector(configuration, instance, std::move(sc_callbacks)));
}

Enabled_server_connector::Uptr Connector_factory::create_and_enable(
    Server_service_interface_definition const& configuration, Service_instance const& instance,
    Optional_reference<Server_connector_credentials_callbacks_mock> sc_callbacks,
    Posix_credentials const& credentials) {
    return Disabled_server_connector::enable(
        create_server_connector(configuration, instance, std::move(sc_callbacks), credentials));
}

Client_connector::Uptr Connector_factory::create_client_connector(
    Optional_reference<Client_connector_callbacks_mock> cc_callbacks) {
    auto cc = create_client_connector(m_configuration, m_instance, std::move(cc_callbacks));
    return cc;
}

Client_connector::Uptr Connector_factory::create_client_connector(
    Client_connector::Callbacks cc_callbacks) {
    auto cc =
        get_runtime().make_client_connector(m_configuration, m_instance, std::move(cc_callbacks));
    EXPECT_TRUE(cc);
    return std::move(cc).value();
}

Client_connector::Uptr Connector_factory::create_client_connector(
    Service_interface_definition const& configuration, Service_instance const& instance,
    Optional_reference<Client_connector_callbacks_mock> cc_callbacks) {
    auto cc =
        create_client_connector_with_result(configuration, instance, std::move(cc_callbacks), {});
    EXPECT_TRUE(cc);
    return std::move(cc).value();
}

Client_connector::Uptr Connector_factory::create_client_connector(
    Service_interface_definition const& configuration, Service_instance const& instance,
    Optional_reference<Client_connector_callbacks_mock> cc_callbacks,
    Posix_credentials const& credentials) {
    auto cc = create_client_connector_with_result(configuration, instance, std::move(cc_callbacks),
                                                  credentials);
    EXPECT_TRUE(cc);
    return std::move(cc).value();
}

score::Result<Client_connector::Uptr> Connector_factory::create_client_connector_with_result(
    Service_interface_definition const& configuration, Service_instance const& instance,
    Optional_reference<Client_connector_callbacks_mock> cc_callbacks,
    std::optional<Posix_credentials> const& credentials) {
    if (credentials) {
        if (cc_callbacks) {
            return create_client_connector_with_result(
                configuration, instance, create_client_callbacks(*cc_callbacks), *credentials);
        }

        return create_client_connector_with_result(configuration, instance,
                                                   Client_connector::Callbacks{}, *credentials);
    }

    if (cc_callbacks) {
        return create_client_connector_with_result(configuration, instance,
                                                   create_client_callbacks(*cc_callbacks));
    }

    return create_client_connector_with_result(configuration, instance,
                                               Client_connector::Callbacks{});
}

score::Result<Client_connector::Uptr> Connector_factory::create_client_connector_with_result(
    Service_interface_definition const& configuration, Service_instance const& instance,
    Client_connector::Callbacks cc_callbacks) {
    return get_runtime().make_client_connector(configuration, instance, std::move(cc_callbacks));
}

score::Result<Client_connector::Uptr> Connector_factory::create_client_connector_with_result(
    Service_interface_definition const& configuration, Service_instance const& instance,
    Client_connector::Callbacks cc_callbacks, Posix_credentials const& credentials) {
    return get_runtime().make_client_connector(configuration, instance, std::move(cc_callbacks),
                                               credentials);
}

Client_connector::Uptr Connector_factory::create_and_connect(
    Optional_reference<Client_connector_callbacks_mock> cc_callbacks) {
    return create_and_connect(m_configuration, m_instance, std::move(cc_callbacks),
                              std::optional<Posix_credentials>{});
}

Client_connector::Uptr Connector_factory::create_and_connect(
    Service_interface_definition const& configuration, Service_instance const& instance,
    Optional_reference<Client_connector_callbacks_mock> cc_callbacks,
    std::optional<Posix_credentials> const& credentials) {
    std::atomic<bool> client_available{!cc_callbacks};
    Client_connector const* available_connector = nullptr;
    if (cc_callbacks) {
        EXPECT_CALL(*cc_callbacks, on_service_state_change(_, Service_state::available, _))
            .WillOnce(DoAll([&available_connector](
                                auto const& cc, auto /*service_state*/,
                                auto const& /*configuration*/) { available_connector = &cc; },
                            Assign(&client_available, true)));
    }
    auto cc = credentials.has_value()
                  ? create_client_connector(configuration, instance, cc_callbacks, *credentials)
                  : create_client_connector(configuration, instance, cc_callbacks);

    wait_for_atomics(client_available);
    if (cc_callbacks) {
        // implementation behind available connector is shortly lived and will be destructed after
        // callback returns
        EXPECT_NE(available_connector, nullptr);
        EXPECT_EQ(available_connector, cc.get());
    }

    return cc;
}

Server_service_interface_definition const& Connector_factory::get_configuration() const {
    return m_configuration;
}

Service_instance const& Connector_factory::get_instance() const { return m_instance; }

std::size_t Connector_factory::get_num_methods() const noexcept {
    return m_configuration.get_num_methods();
}

std::size_t Connector_factory::get_num_events() const noexcept {
    return m_configuration.get_num_events();
}

}  // namespace score::socom
