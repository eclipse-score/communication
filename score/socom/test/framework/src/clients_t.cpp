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

#include <atomic>
#include <cstddef>
#include <future>
#include <memory>

#include "gmock/gmock.h"
#include "score/socom/client_connector.hpp"
#include "score/socom/event.hpp"
#include "score/socom/method.hpp"
#include "score/socom/utilities.hpp"
#include "score/socom/vector_payload.hpp"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Assign;
using ::testing::ByMove;
using ::testing::Return;
using ::testing::Truly;

namespace score::socom {
namespace {

void maybe_connect(Client_connector_callbacks_mock& callbacks,
                   Client_data::No_connect_helper const& connect_helper,
                   Service_state_change_callback state_change_callback) {
    if (Client_data::might_connect == connect_helper && !state_change_callback.empty()) {
        auto callback =
            std::make_shared<Service_state_change_callback>(std::move(state_change_callback));
        EXPECT_CALL(callbacks, on_service_state_change(_, _, _))
            .Times(AnyNumber())
            .WillRepeatedly([callback](auto const& connector, auto state, auto const& conf) {
                (*callback)(connector, state, conf);
            });
    }
}

Client_connector::Uptr create_and_maybe_connect(
    Client_connector_callbacks_mock& callbacks, Connector_factory& factory,
    Service_interface_definition const& configuration, Service_instance const& instance,
    Client_data::No_connect_helper const& connect_helper,
    Service_state_change_callback state_change_callback) {
    maybe_connect(callbacks, connect_helper, std::move(state_change_callback));
    return factory.create_client_connector(configuration, instance, callbacks);
}

Client_connector::Uptr create_and_maybe_connect(
    Client_connector_callbacks_mock& callbacks, Connector_factory& factory,
    Client_data::No_connect_helper const& connect_helper,
    Service_state_change_callback state_change_callback) {
    maybe_connect(callbacks, connect_helper, std::move(state_change_callback));
    return factory.create_client_connector(callbacks);
}

}  // namespace

static_assert(!std::is_default_constructible<Client_data>::value, "");

Client_data::Client_data(Connector_factory& factory)
    : m_connector{factory.create_and_connect(m_callbacks)} {}

Client_data::Client_data(Connector_factory& factory, No_connect_helper const& connect_helper,
                         Service_state_change_callback state_change_callback)
    : m_connector{create_and_maybe_connect(m_callbacks, factory, connect_helper,
                                           std::move(state_change_callback))} {}

Client_data::Client_data(Connector_factory& factory, No_connect_helper const& connect_helper,
                         Service_interface_definition const& configuration,
                         Service_instance const& instance,
                         Service_state_change_callback state_change_callback)
    : m_connector{create_and_maybe_connect(m_callbacks, factory, configuration, instance,
                                           connect_helper, std::move(state_change_callback))} {}

Client_data::Client_data(Connector_factory& factory,
                         Service_interface_definition const& configuration,
                         Service_instance const& instance,
                         std::optional<Posix_credentials> const& credentials)
    : m_connector{factory.create_and_connect(configuration, instance, m_callbacks, credentials)} {}

Client_data::~Client_data() {
    wait_for_atomics(m_event_callback_called, m_event_request_callback_called,
                     m_method_callback_called, m_available, m_not_available,
                     m_event_subscription_status_change_called, m_event_payload_allocate_called);
}

void Client_data::subscribe_event(Event_id const& event_id, Event_mode const mode) {
    m_connector->subscribe_event(event_id, mode);
}

void Client_data::unsubscribe_event(Event_id const& event_id) {
    m_connector->unsubscribe_event(event_id);
}

std::unique_ptr<Temporary_event_subscription> Client_data::create_event_subscription(
    Event_id const& event_id) {
    return std::make_unique<Temporary_event_subscription>(*m_connector, event_id);
}

std::unique_ptr<Temporary_event_subscription> Client_data::create_event_subscription(
    Server_data& server, Event_id const& event_id,
    Temporary_event_subscription::Brokenness const& brokenness) {
    return std::make_unique<Temporary_event_subscription>(*m_connector, server.get_callbacks(),
                                                          event_id, brokenness);
}

void Client_data::request_event_update(Event_id const& event_id) const {
    m_connector->request_event_update(event_id);
}

score::Result<Writable_payload> Client_data::allocate_method_call_payload(Method_id method_id) {
    return m_connector->allocate_method_call_payload(method_id);
}

void Client_data::call_method(Method_id const& method_id, Payload const& payload) {
    auto result = m_connector->call_method(
        method_id, clone_payload(payload),
        Method_call_reply_data{m_method_callback.as_function(), std::nullopt});
    ASSERT_TRUE(result);
    m_method_invocations.emplace_back(std::move(result).value());
}

void Client_data::call_method(Method_id const& method_id, Payload const& payload,
                              Method_reply_callback reply) {
    call_method(method_id, payload, Method_call_reply_data{std::move(reply), std::nullopt});
}

void Client_data::call_method(Method_id const& method_id, Payload const& payload,
                              Method_call_reply_data reply) {
    auto result = m_connector->call_method(method_id, clone_payload(payload), std::move(reply));
    ASSERT_TRUE(result);
    m_method_invocations.emplace_back(std::move(result).value());
}

void Client_data::call_method_fire_and_forget(Method_id const& method_id, Payload const& payload) {
    EXPECT_TRUE(m_method_callback_called);
    m_method_invocations.clear();
    auto result = m_connector->call_method(method_id, clone_payload(payload));
    ASSERT_TRUE(result);
    m_method_invocations.emplace_back(std::move(result).value());
}

score::Result<Method_invocation::Uptr>
Client_data::call_method_fire_and_forget_and_return_invocation(Method_id const& method_id,
                                                               Payload const& payload) {
    return m_connector->call_method(method_id, clone_payload(payload));
}

std::atomic<bool> const& Client_data::expect_service_state_change(Service_state const& state) {
    return expect_service_state_change(1, state);
}

std::atomic<bool>& Client_data::get_atomic(Service_state const& state) {
    auto& atomi = Service_state::available == state ? m_available : m_not_available;
    return atomi;
}

std::atomic<bool> const& Client_data::expect_service_state_change(size_t const count,
                                                                  Service_state const& state) {
    Optional_reference<Server_service_interface_definition const> const conf;
    return expect_service_state_change(count, state, conf);
}

std::atomic<bool> const& Client_data::expect_service_state_change(
    size_t const count, Service_state const& state,
    Optional_reference<Server_service_interface_definition const> const& conf) {
    auto& atomi = get_atomic(state);
    EXPECT_TRUE(atomi);
    atomi = false;
    if (conf) {
        EXPECT_CALL(m_callbacks, on_service_state_change(_, state, *conf))
            .Times(count)
            .WillRepeatedly(Assign(&atomi, true));
    } else {
        EXPECT_CALL(m_callbacks, on_service_state_change(_, state, _))
            .Times(count)
            .WillRepeatedly(Assign(&atomi, true));
    }
    return atomi;
}

std::atomic<bool> const& Client_data::expect_event_payload_allocation(
    Event_id const& event_id, score::Result<Writable_payload> result) {
    EXPECT_CALL(m_callbacks, on_event_payload_allocate(_, event_id))
        .WillOnce(DoAll(Assign(&m_event_payload_allocate_called, true),
                        Return(ByMove(std::move(result)))));

    m_event_payload_allocate_called = false;
    return m_event_payload_allocate_called;
}

std::atomic<bool> const& Client_data::expect_event_update(Event_id const& event_id,
                                                          Payload const& payload) {
    return expect_event_updates(1, event_id, payload);
}

std::atomic<bool> const& Client_data::expect_event_updates(size_t const& count,
                                                           Event_id const& event_id,
                                                           Payload const& payload) {
    auto const check_update_count = [this, count](auto const& /*cc*/, auto /*event_id*/,
                                                  auto const& /*payload*/) {
        m_num_event_callback_called++;
        if (count == m_num_event_callback_called) {
            m_event_callback_called = true;
        }
    };

    EXPECT_TRUE(m_event_callback_called);
    m_event_callback_called = false;
    m_num_event_callback_called = 0;
    EXPECT_CALL(m_callbacks, on_event_update(_, event_id, payload_eq(payload)))
        .Times(count)
        .WillRepeatedly(check_update_count);
    return m_event_callback_called;
}

std::future<void> Client_data::expect_event_updates_min_number(std::size_t const& count,
                                                               Event_id const& event_id,
                                                               Payload const& payload) {
    std::promise<void> event_received;
    auto future = event_received.get_future();

    auto const check_update_count =
        create_check_update_count(m_num_event_callback_called, count, std::move(event_received));

    EXPECT_CALL(m_callbacks, on_event_update(_, event_id, payload_eq(payload)))
        .WillRepeatedly(check_update_count);
    return future;
}

std::atomic<bool> const& Client_data::expect_requested_event_update(Event_id const& event_id,
                                                                    Payload const& payload) {
    EXPECT_TRUE(m_event_request_callback_called);
    m_event_request_callback_called = false;
    EXPECT_CALL(m_callbacks, on_requested_event_update(_, event_id, payload_eq(payload)))
        .WillOnce(Assign(&m_event_request_callback_called, true));
    return m_event_request_callback_called;
}

std::future<void> Client_data::expect_requested_event_updates_min_number(std::size_t const& count,
                                                                         Event_id const& event_id,
                                                                         Payload const& payload) {
    std::promise<void> event_received;
    auto future = event_received.get_future();

    auto const check_update_count =
        create_check_update_count(m_num_event_callback_called, count, std::move(event_received));

    EXPECT_CALL(m_callbacks, on_requested_event_update(_, event_id, payload_eq(payload)))
        .WillRepeatedly(check_update_count);
    return future;
}

std::atomic<bool> const& Client_data::expect_and_request_event_update(Event_id const& event_id,
                                                                      Payload const& payload) {
    auto const& cb_called = expect_requested_event_update(event_id, payload);
    request_event_update(event_id);
    return cb_called;
}

std::atomic<bool> const& Client_data::expect_and_call_method(Method_id const& method_id,
                                                             Payload const& payload,
                                                             Method_result const& method_result) {
    return expect_and_call_methods(1, method_id, payload, method_result);
}

std::atomic<bool> const& Client_data::expect_and_call_methods(size_t const& count,
                                                              Method_id const& method_id,
                                                              Payload const& payload,
                                                              Method_result const& method_result) {
    auto const check_update_count = [this, count](auto const& /*method_result*/) {
        m_num_method_callback_called++;
        if (count == m_num_method_callback_called) {
            m_method_callback_called = true;
        }
    };

    EXPECT_TRUE(m_method_callback_called);
    m_method_invocations.clear();
    m_method_callback_called = false;
    m_num_method_callback_called = 0;
    EXPECT_CALL(
        m_method_callback,
        Call(Truly([&method_result](auto const& result) { return result == method_result; })))
        .Times(count)
        .WillRepeatedly(check_update_count);
    for (auto i = size_t{0}; i < count; i++) {
        call_method(method_id, payload);
    }
    return m_method_callback_called;
}

Client_data::Vector Client_data::create_clients(Connector_factory& factory, size_t const& size) {
    return create_clients(factory, size, factory.get_configuration(), factory.get_instance());
}

Client_data::Vector Client_data::create_clients(Connector_factory& factory, size_t const& size,
                                                No_connect_helper const& no_connect) {
    auto result = Client_data::Vector{size};
    for (auto& item : result) {
        item = std::make_unique<Client_data>(factory, no_connect);
    }
    return result;
}

Client_data::Vector Client_data::create_clients(Connector_factory& factory, size_t const& size,
                                                Service_interface_definition const& configuration,
                                                Service_instance const& instance) {
    auto result = Client_data::Vector{size};
    for (auto& item : result) {
        item = std::make_unique<Client_data>(factory, configuration, instance);
    }
    return result;
}

Callbacks_called_t Client_data::expect_event_update(Vector& clients, Event_id const event_id,
                                                    Payload const& payload) {
    auto cb = Callbacks_called_t{};
    for (auto& cc_cb : clients) {
        if (nullptr == cc_cb) {
            break;
        }
        auto const& cb_call = cc_cb->expect_event_update(event_id, payload);
        cb.emplace_back(cb_call);
    }
    return cb;
}

Callbacks_called_t Client_data::expect_and_request_event_update(Vector& clients,
                                                                Event_id const event_id,
                                                                Payload const& payload) {
    auto cb_called = Callbacks_called_t{};
    for (auto& client : clients) {
        if (nullptr == client) {
            break;
        }
        auto const& cb_call = client->expect_and_request_event_update(event_id, payload);
        cb_called.emplace_back(cb_call);
    }
    return cb_called;
}

Callbacks_called_t Client_data::expect_and_call_method(Vector& clients, Method_id const method_id,
                                                       Payload const& payload,
                                                       Method_result const& method_result) {
    auto cb_called = Callbacks_called_t{};
    for (auto& client : clients) {
        if (nullptr == client) {
            break;
        }
        auto const& cb_call = client->expect_and_call_method(method_id, payload, method_result);
        cb_called.emplace_back(cb_call);
    }
    return cb_called;
}

Subscriptions Client_data::subscribe(Client_data::Vector const& clients, Event_id const& event_id) {
    auto result = Subscriptions{};
    result.reserve(clients.size());
    for (auto& item : clients) {
        result.emplace_back(item->create_event_subscription(event_id));
    }
    return result;
}

score::Result<Posix_credentials> Client_data::get_peer_credentials() const {
    return m_connector->get_peer_credentials();
}

}  // namespace score::socom
