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

#include "score/socom/clients_t.hpp"

#include <gtest/gtest.h>
#include <atomic>
#include <cstddef>
#include <future>
#include <memory>

#include "gmock/gmock.h"
#include "score/result/result.h"
#include "score/socom/client_connector.hpp"
#include "score/socom/event.hpp"
#include "score/socom/method.hpp"
#include "score/socom/payload.hpp"
#include "score/socom/posix_credentials.hpp"
#include "score/socom/service_interface_definition.hpp"
#include "score/socom/service_interface_identifier.hpp"
#include "score/socom/utilities.hpp"
#include "score/socom/vector_payload.hpp"
#include <score/socom/connector_factory.hpp>
#include <score/socom/server_t.hpp>
#include <score/socom/socom_mocks.hpp>
#include <score/socom/temporary_event_subscription.hpp>
#include <optional>
#include <type_traits>
#include <utility>

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Assign;
using ::testing::ByMove;
using ::testing::Return;
using ::testing::Truly;

namespace score::socom
{
namespace
{

void maybe_connect(Client_connector_callbacks_mock& callbacks,
                   const Client_data::No_connect_helper& connect_helper,
                   Service_state_change_callback state_change_callback)
{
    if (Client_data::might_connect == connect_helper && !state_change_callback.empty())
    {
        auto callback = std::make_shared<Service_state_change_callback>(std::move(state_change_callback));
        EXPECT_CALL(callbacks, on_service_state_change(_, _, _))
            .Times(AnyNumber())
            .WillRepeatedly([callback](const auto& connector, auto state, const auto& conf) {
                (*callback)(connector, state, conf);
            });
    }
}

Client_connector::Uptr create_and_maybe_connect(Client_connector_callbacks_mock& callbacks,
                                                Connector_factory& factory,
                                                const Service_interface_definition& configuration,
                                                const Service_instance& instance,
                                                const Client_data::No_connect_helper& connect_helper,
                                                Service_state_change_callback state_change_callback)
{
    maybe_connect(callbacks, connect_helper, std::move(state_change_callback));
    return factory.create_client_connector(configuration, instance, callbacks);
}

Client_connector::Uptr create_and_maybe_connect(Client_connector_callbacks_mock& callbacks,
                                                Connector_factory& factory,
                                                const Client_data::No_connect_helper& connect_helper,
                                                Service_state_change_callback state_change_callback)
{
    maybe_connect(callbacks, connect_helper, std::move(state_change_callback));
    return factory.create_client_connector(callbacks);
}

}  // namespace

static_assert(!std::is_default_constructible<Client_data>::value, "");

Client_data::Client_data(Connector_factory& factory) : m_connector{factory.create_and_connect(m_callbacks)} {}

Client_data::Client_data(Connector_factory& factory,
                         const No_connect_helper& connect_helper,
                         Service_state_change_callback state_change_callback)
    : m_connector{create_and_maybe_connect(m_callbacks, factory, connect_helper, std::move(state_change_callback))}
{
}

Client_data::Client_data(Connector_factory& factory,
                         const No_connect_helper& connect_helper,
                         const Service_interface_definition& configuration,
                         const Service_instance& instance,
                         Service_state_change_callback state_change_callback)
    : m_connector{create_and_maybe_connect(m_callbacks,
                                           factory,
                                           configuration,
                                           instance,
                                           connect_helper,
                                           std::move(state_change_callback))}
{
}

Client_data::Client_data(Connector_factory& factory,
                         const Service_interface_definition& configuration,
                         const Service_instance& instance,
                         const std::optional<Posix_credentials>& credentials)
    : m_connector{factory.create_and_connect(configuration, instance, m_callbacks, credentials)}
{
}

Client_data::~Client_data()
{
    wait_for_atomics(m_event_callback_called,
                     m_event_request_callback_called,
                     m_method_callback_called,
                     m_available,
                     m_not_available,
                     m_event_subscription_status_change_called,
                     m_event_payload_allocate_called);
}

void Client_data::subscribe_event(const Event_id& event_id, const Event_mode mode)
{
    m_connector->subscribe_event(event_id, mode);
}

void Client_data::unsubscribe_event(const Event_id& event_id)
{
    m_connector->unsubscribe_event(event_id);
}

std::unique_ptr<Temporary_event_subscription> Client_data::create_event_subscription(const Event_id& event_id)
{
    return std::make_unique<Temporary_event_subscription>(*m_connector, event_id);
}

std::unique_ptr<Temporary_event_subscription> Client_data::create_event_subscription(
    Server_data& server,
    const Event_id& event_id,
    const Temporary_event_subscription::Brokenness& brokenness)
{
    return std::make_unique<Temporary_event_subscription>(*m_connector, server.get_callbacks(), event_id, brokenness);
}

void Client_data::request_event_update(const Event_id& event_id) const
{
    m_connector->request_event_update(event_id);
}

score::Result<Writable_payload> Client_data::allocate_method_call_payload(Method_id method_id)
{
    return m_connector->allocate_method_call_payload(method_id);
}

void Client_data::call_method(const Method_id& method_id, const Payload& payload)
{
    auto result = m_connector->call_method(
        method_id, clone_payload(payload), Method_call_reply_data{m_method_callback.as_function(), std::nullopt});
    ASSERT_TRUE(result);
    m_method_invocations.emplace_back(std::move(result).value());
}

void Client_data::call_method(const Method_id& method_id, const Payload& payload, Method_reply_callback reply)
{
    call_method(method_id, payload, Method_call_reply_data{std::move(reply), std::nullopt});
}

void Client_data::call_method(const Method_id& method_id, const Payload& payload, Method_call_reply_data reply)
{
    auto result = m_connector->call_method(method_id, clone_payload(payload), std::move(reply));
    ASSERT_TRUE(result);
    m_method_invocations.emplace_back(std::move(result).value());
}

void Client_data::call_method_fire_and_forget(const Method_id& method_id, const Payload& payload)
{
    EXPECT_TRUE(m_method_callback_called);
    m_method_invocations.clear();
    auto result = m_connector->call_method(method_id, clone_payload(payload));
    ASSERT_TRUE(result);
    m_method_invocations.emplace_back(std::move(result).value());
}

score::Result<Method_invocation::Uptr> Client_data::call_method_fire_and_forget_and_return_invocation(
    const Method_id& method_id,
    const Payload& payload)
{
    return m_connector->call_method(method_id, clone_payload(payload));
}

const std::atomic<bool>& Client_data::expect_service_state_change(const Service_state& state)
{
    return expect_service_state_change(1, state);
}

std::atomic<bool>& Client_data::get_atomic(const Service_state& state)
{
    auto& atomi = Service_state::available == state ? m_available : m_not_available;
    return atomi;
}

const std::atomic<bool>& Client_data::expect_service_state_change(const size_t count, const Service_state& state)
{
    const Optional_reference<const Server_service_interface_definition> conf;
    return expect_service_state_change(count, state, conf);
}

const std::atomic<bool>& Client_data::expect_service_state_change(
    const size_t count,
    const Service_state& state,
    const Optional_reference<const Server_service_interface_definition>& conf)
{
    auto& atomi = get_atomic(state);
    EXPECT_TRUE(atomi);
    atomi = false;
    if (conf)
    {
        EXPECT_CALL(m_callbacks, on_service_state_change(_, state, *conf))
            .Times(to_int(count))
            .WillRepeatedly(Assign(&atomi, true));
    }
    else
    {
        EXPECT_CALL(m_callbacks, on_service_state_change(_, state, _))
            .Times(to_int(count))
            .WillRepeatedly(Assign(&atomi, true));
    }
    return atomi;
}

const std::atomic<bool>& Client_data::expect_event_payload_allocation(const Event_id& event_id,
                                                                      score::Result<Writable_payload> result)
{
    EXPECT_CALL(m_callbacks, on_event_payload_allocate(_, event_id))
        .WillOnce(DoAll(Assign(&m_event_payload_allocate_called, true), Return(ByMove(std::move(result)))));

    m_event_payload_allocate_called = false;
    return m_event_payload_allocate_called;
}

const std::atomic<bool>& Client_data::expect_event_update(const Event_id& event_id, const Payload& payload)
{
    return expect_event_updates(1, event_id, payload);
}

const std::atomic<bool>& Client_data::expect_event_updates(const size_t& count,
                                                           const Event_id& event_id,
                                                           const Payload& payload)
{
    const auto check_update_count = [this, count](const auto& /*cc*/, auto /*event_id*/, const auto& /*payload*/) {
        m_num_event_callback_called++;
        if (count == m_num_event_callback_called)
        {
            m_event_callback_called = true;
        }
    };

    EXPECT_TRUE(m_event_callback_called);
    m_event_callback_called = false;
    m_num_event_callback_called = 0;
    EXPECT_CALL(m_callbacks, on_event_update(_, event_id, payload_eq(payload)))
        .Times(to_int(count))
        .WillRepeatedly(check_update_count);
    return m_event_callback_called;
}

std::future<void> Client_data::expect_event_updates_min_number(const std::size_t& count,
                                                               const Event_id& event_id,
                                                               const Payload& payload)
{
    std::promise<void> event_received;
    auto future = event_received.get_future();

    const auto check_update_count =
        create_check_update_count(m_num_event_callback_called, count, std::move(event_received));

    EXPECT_CALL(m_callbacks, on_event_update(_, event_id, payload_eq(payload))).WillRepeatedly(check_update_count);
    return future;
}

const std::atomic<bool>& Client_data::expect_requested_event_update(const Event_id& event_id, const Payload& payload)
{
    EXPECT_TRUE(m_event_request_callback_called);
    m_event_request_callback_called = false;
    EXPECT_CALL(m_callbacks, on_requested_event_update(_, event_id, payload_eq(payload)))
        .WillOnce(Assign(&m_event_request_callback_called, true));
    return m_event_request_callback_called;
}

std::future<void> Client_data::expect_requested_event_updates_min_number(const std::size_t& count,
                                                                         const Event_id& event_id,
                                                                         const Payload& payload)
{
    std::promise<void> event_received;
    auto future = event_received.get_future();

    const auto check_update_count =
        create_check_update_count(m_num_event_callback_called, count, std::move(event_received));

    EXPECT_CALL(m_callbacks, on_requested_event_update(_, event_id, payload_eq(payload)))
        .WillRepeatedly(check_update_count);
    return future;
}

const std::atomic<bool>& Client_data::expect_and_request_event_update(const Event_id& event_id, const Payload& payload)
{
    const auto& cb_called = expect_requested_event_update(event_id, payload);
    request_event_update(event_id);
    return cb_called;
}

const std::atomic<bool>& Client_data::expect_and_call_method(const Method_id& method_id,
                                                             const Payload& payload,
                                                             const Method_result& method_result)
{
    return expect_and_call_methods(1, method_id, payload, method_result);
}

const std::atomic<bool>& Client_data::expect_and_call_methods(const size_t& count,
                                                              const Method_id& method_id,
                                                              const Payload& payload,
                                                              const Method_result& method_result)
{
    const auto check_update_count = [this, count](const auto& /*method_result*/) {
        m_num_method_callback_called++;
        if (count == m_num_method_callback_called)
        {
            m_method_callback_called = true;
        }
    };

    EXPECT_TRUE(m_method_callback_called);
    m_method_invocations.clear();
    m_method_callback_called = false;
    m_num_method_callback_called = 0;
    EXPECT_CALL(m_method_callback, Call(Truly([&method_result](const auto& result) {
                    return result == method_result;
                })))
        .Times(to_int(count))
        .WillRepeatedly(check_update_count);
    for (auto i = size_t{0}; i < count; i++)
    {
        call_method(method_id, payload);
    }
    return m_method_callback_called;
}

Client_data::Vector Client_data::create_clients(Connector_factory& factory, const size_t& size)
{
    return create_clients(factory, size, factory.get_configuration(), factory.get_instance());
}

Client_data::Vector Client_data::create_clients(Connector_factory& factory,
                                                const size_t& size,
                                                const No_connect_helper& no_connect)
{
    auto result = Client_data::Vector{size};
    for (auto& item : result)
    {
        item = std::make_unique<Client_data>(factory, no_connect);
    }
    return result;
}

Client_data::Vector Client_data::create_clients(Connector_factory& factory,
                                                const size_t& size,
                                                const Service_interface_definition& configuration,
                                                const Service_instance& instance)
{
    auto result = Client_data::Vector{size};
    for (auto& item : result)
    {
        item = std::make_unique<Client_data>(factory, configuration, instance);
    }
    return result;
}

Callbacks_called_t Client_data::expect_event_update(Vector& clients, const Event_id event_id, const Payload& payload)
{
    auto cb = Callbacks_called_t{};
    for (auto& cc_cb : clients)
    {
        if (nullptr == cc_cb)
        {
            break;
        }
        const auto& cb_call = cc_cb->expect_event_update(event_id, payload);
        cb.emplace_back(cb_call);
    }
    return cb;
}

Callbacks_called_t Client_data::expect_and_request_event_update(Vector& clients,
                                                                const Event_id event_id,
                                                                const Payload& payload)
{
    auto cb_called = Callbacks_called_t{};
    for (auto& client : clients)
    {
        if (nullptr == client)
        {
            break;
        }
        const auto& cb_call = client->expect_and_request_event_update(event_id, payload);
        cb_called.emplace_back(cb_call);
    }
    return cb_called;
}

Callbacks_called_t Client_data::expect_and_call_method(Vector& clients,
                                                       const Method_id method_id,
                                                       const Payload& payload,
                                                       const Method_result& method_result)
{
    auto cb_called = Callbacks_called_t{};
    for (auto& client : clients)
    {
        if (nullptr == client)
        {
            break;
        }
        const auto& cb_call = client->expect_and_call_method(method_id, payload, method_result);
        cb_called.emplace_back(cb_call);
    }
    return cb_called;
}

Subscriptions Client_data::subscribe(const Client_data::Vector& clients, const Event_id& event_id)
{
    auto result = Subscriptions{};
    result.reserve(clients.size());
    for (const auto& item : clients)
    {
        result.emplace_back(item->create_event_subscription(event_id));
    }
    return result;
}

score::Result<Posix_credentials> Client_data::get_peer_credentials() const
{
    return m_connector->get_peer_credentials();
}

}  // namespace score::socom
