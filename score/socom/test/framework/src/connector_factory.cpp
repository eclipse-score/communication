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

#include "gmock/gmock.h"
#include <atomic>
#include <cstddef>
#include <optional>

#include "gtest/gtest.h"
#include "score/result/result.h"
#include "score/socom/client_connector.hpp"
#include "score/socom/posix_credentials.hpp"
#include "score/socom/runtime.hpp"
#include "score/socom/server_connector.hpp"
#include "score/socom/service_interface_definition.hpp"
#include "score/socom/service_interface_identifier.hpp"
#include "score/socom/utilities.hpp"
#include <score/socom/socom_mocks.hpp>
#include <utility>

using ::testing::_;
using ::testing::Assign;
using ::testing::DoAll;

namespace score::socom
{

Connector_factory::Connector_factory(Server_service_interface_definition configuration, Service_instance instance)
    : m_runtime{create_runtime()}, m_configuration{std::move(configuration)}, m_instance{instance}
{
}

Connector_factory::Connector_factory(const Service_interface_identifier& sif,
                                     const Num_of_methods num_methods,
                                     const Num_of_events num_events,
                                     Service_instance instance)
    : Connector_factory{Server_service_interface_definition{sif, num_methods, num_events}, instance}
{
}

Connector_factory::Connector_factory(const Connector_factory& con_fac)
    : m_runtime{create_runtime()}, m_configuration{con_fac.get_configuration()}, m_instance{con_fac.get_instance()}
{
}

Runtime& Connector_factory::get_runtime()
{
    return *m_runtime;
}

score::Result<Service_bridge_registration> Connector_factory::register_service_bridge(
    Bridge_identity identity,
    Request_service_function request_service)
{
    return m_runtime->register_service_bridge(identity, std::move(request_service));
}

Disabled_server_connector::Uptr Connector_factory::create_server_connector(
    Optional_reference<Server_connector_callbacks_mock> sc_callbacks)
{
    return create_server_connector(m_configuration, m_instance, sc_callbacks);
}

Disabled_server_connector::Uptr Connector_factory::create_server_connector(
    Disabled_server_connector::Callbacks sc_callbacks)
{
    auto sc = create_server_connector_with_result(std::move(sc_callbacks));
    EXPECT_TRUE(sc);
    return std::move(sc).value();
}

score::Result<Disabled_server_connector::Uptr> Connector_factory::create_server_connector_with_result(
    Disabled_server_connector::Callbacks sc_callbacks)
{
    return get_runtime().make_server_connector(m_configuration, m_instance, std::move(sc_callbacks));
}

score::Result<Disabled_server_connector::Uptr> Connector_factory::create_server_connector_with_result(
    Optional_reference<Server_connector_callbacks_mock> sc_callbacks)
{
    if (sc_callbacks)
    {
        return create_server_connector_with_result(create_server_callbacks(*sc_callbacks));
    }

    return create_server_connector_with_result(Disabled_server_connector::Callbacks{});
}

Disabled_server_connector::Uptr Connector_factory::create_server_connector(
    const Server_service_interface_definition& configuration,
    const Service_instance& instance,
    Optional_reference<Server_connector_callbacks_mock> sc_callbacks)
{
    auto callbacks = sc_callbacks ? create_server_callbacks(*sc_callbacks) : Disabled_server_connector::Callbacks{};
    auto sc = get_runtime().make_server_connector(configuration, instance, std::move(callbacks));
    EXPECT_TRUE(sc);
    return std::move(sc).value();
}

Disabled_server_connector::Uptr Connector_factory::create_server_connector(
    const Server_service_interface_definition& configuration,
    const Service_instance& instance,
    Optional_reference<Server_connector_credentials_callbacks_mock> sc_callbacks,
    const Posix_credentials& credentials)
{
    auto callbacks = sc_callbacks ? create_server_callbacks(*sc_callbacks) : Disabled_server_connector::Callbacks{};
    auto sc = get_runtime().make_server_connector(configuration, instance, std::move(callbacks), credentials);
    EXPECT_TRUE(sc);
    return std::move(sc).value();
}

Enabled_server_connector::Uptr Connector_factory::create_and_enable(
    Optional_reference<Server_connector_callbacks_mock> sc_callbacks)
{
    return create_and_enable(m_configuration, m_instance, sc_callbacks);
}

Enabled_server_connector::Uptr Connector_factory::create_and_enable(
    const Server_service_interface_definition& configuration,
    const Service_instance& instance,
    Optional_reference<Server_connector_callbacks_mock> sc_callbacks)
{
    return Disabled_server_connector::enable(create_server_connector(configuration, instance, sc_callbacks));
}

Enabled_server_connector::Uptr Connector_factory::create_and_enable(
    const Server_service_interface_definition& configuration,
    const Service_instance& instance,
    Optional_reference<Server_connector_credentials_callbacks_mock> sc_callbacks,
    const Posix_credentials& credentials)
{
    return Disabled_server_connector::enable(
        create_server_connector(configuration, instance, sc_callbacks, credentials));
}

Client_connector::Uptr Connector_factory::create_client_connector(
    Optional_reference<Client_connector_callbacks_mock> cc_callbacks)
{
    auto cc = create_client_connector(m_configuration, m_instance, cc_callbacks);
    return cc;
}

Client_connector::Uptr Connector_factory::create_client_connector(Client_connector::Callbacks cc_callbacks)
{
    auto cc = get_runtime().make_client_connector(m_configuration, m_instance, std::move(cc_callbacks));
    EXPECT_TRUE(cc);
    return std::move(cc).value();
}

Client_connector::Uptr Connector_factory::create_client_connector(
    const Service_interface_definition& configuration,
    const Service_instance& instance,
    Optional_reference<Client_connector_callbacks_mock> cc_callbacks)
{
    auto cc = create_client_connector_with_result(configuration, instance, cc_callbacks, {});
    EXPECT_TRUE(cc);
    return std::move(cc).value();
}

Client_connector::Uptr Connector_factory::create_client_connector(
    const Service_interface_definition& configuration,
    const Service_instance& instance,
    Optional_reference<Client_connector_callbacks_mock> cc_callbacks,
    const Posix_credentials& credentials)
{
    auto cc = create_client_connector_with_result(configuration, instance, cc_callbacks, credentials);
    EXPECT_TRUE(cc);
    return std::move(cc).value();
}

score::Result<Client_connector::Uptr> Connector_factory::create_client_connector_with_result(
    const Service_interface_definition& configuration,
    const Service_instance& instance,
    Optional_reference<Client_connector_callbacks_mock> cc_callbacks,
    const std::optional<Posix_credentials>& credentials)
{
    if (credentials)
    {
        if (cc_callbacks)
        {
            return create_client_connector_with_result(
                configuration, instance, create_client_callbacks(*cc_callbacks), *credentials);
        }

        return create_client_connector_with_result(
            configuration, instance, Client_connector::Callbacks{}, *credentials);
    }

    if (cc_callbacks)
    {
        return create_client_connector_with_result(configuration, instance, create_client_callbacks(*cc_callbacks));
    }

    return create_client_connector_with_result(configuration, instance, Client_connector::Callbacks{});
}

score::Result<Client_connector::Uptr> Connector_factory::create_client_connector_with_result(
    const Service_interface_definition& configuration,
    const Service_instance& instance,
    Client_connector::Callbacks cc_callbacks)
{
    return get_runtime().make_client_connector(configuration, instance, std::move(cc_callbacks));
}

score::Result<Client_connector::Uptr> Connector_factory::create_client_connector_with_result(
    const Service_interface_definition& configuration,
    const Service_instance& instance,
    Client_connector::Callbacks cc_callbacks,
    const Posix_credentials& credentials)
{
    return get_runtime().make_client_connector(configuration, instance, std::move(cc_callbacks), credentials);
}

Client_connector::Uptr Connector_factory::create_and_connect(
    Optional_reference<Client_connector_callbacks_mock> cc_callbacks)
{
    return create_and_connect(m_configuration, m_instance, cc_callbacks, std::optional<Posix_credentials>{});
}

Client_connector::Uptr Connector_factory::create_and_connect(
    const Service_interface_definition& configuration,
    const Service_instance& instance,
    Optional_reference<Client_connector_callbacks_mock> cc_callbacks,
    const std::optional<Posix_credentials>& credentials)
{
    std::atomic<bool> client_available{!cc_callbacks};
    const Client_connector* available_connector = nullptr;
    if (cc_callbacks)
    {
        EXPECT_CALL(*cc_callbacks, on_service_state_change(_, Service_state::available, _))
            .WillOnce(DoAll(
                [&available_connector](const auto& cc, auto /*service_state*/, const auto& /*configuration*/) {
                    available_connector = &cc;
                },
                Assign(&client_available, true)));
    }
    auto cc = credentials.has_value() ? create_client_connector(configuration, instance, cc_callbacks, *credentials)
                                      : create_client_connector(configuration, instance, cc_callbacks);

    wait_for_atomics(client_available);
    if (cc_callbacks)
    {
        // implementation behind available connector is shortly lived and will be destructed after
        // callback returns
        EXPECT_NE(available_connector, nullptr);
        EXPECT_EQ(available_connector, cc.get());
    }

    return cc;
}

const Server_service_interface_definition& Connector_factory::get_configuration() const
{
    return m_configuration;
}

const Service_instance& Connector_factory::get_instance() const
{
    return m_instance;
}

std::size_t Connector_factory::get_num_methods() const noexcept
{
    return m_configuration.get_num_methods();
}

std::size_t Connector_factory::get_num_events() const noexcept
{
    return m_configuration.get_num_events();
}

}  // namespace score::socom
