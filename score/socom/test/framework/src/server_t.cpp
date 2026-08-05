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

#include "score/socom/server_t.hpp"

#include <atomic>
#include <future>
#include <memory>

#include "score/socom/method.hpp"
#include "score/socom/payload.hpp"
#include "score/socom/server_connector.hpp"
#include "score/socom/socom_mocks.hpp"
#include "score/socom/utilities.hpp"
#include "score/socom/vector_payload.hpp"

using ::testing::_;
using ::testing::Assign;
using ::testing::ByMove;
using ::testing::DoAll;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
namespace score::socom {
namespace {
Server_connector_callbacks_mock& expect_method_call(Server_connector_callbacks_mock& sc_callbacks,
                                                    Method_id const& method_id,
                                                    Payload const& expected_payload) {
    EXPECT_CALL(sc_callbacks, on_method_call(_, method_id, payload_eq(expected_payload), _));
    return sc_callbacks;
}

auto ignore_call() {
    return InvokeWithoutArgs([]() {});
}

}  // namespace

static_assert(!std::is_default_constructible<Server_data>::value, "");

Server_data::Server_data(Connector_factory& factory)
    : m_connector{factory.create_and_enable(m_callbacks)} {}

Server_data::Server_data(Connector_factory& factory, Method_id method_id,
                         Payload const& expected_payload)
    : m_connector{factory.create_and_enable(
          expect_method_call(m_callbacks, method_id, expected_payload))} {}

Server_data::Server_data(Connector_factory& factory,
                         Server_service_interface_definition const& configuration,
                         Service_instance const& instance)
    : m_connector{factory.create_and_enable(configuration, instance, m_callbacks)} {}

Server_data::Server_data(Connector_factory& factory,
                         Server_service_interface_definition const& configuration,
                         Service_instance const& instance, Posix_credentials const& credentials)
    : m_connector{factory.create_and_enable(configuration, instance, m_credential_callbacks,
                                            credentials)} {}

Server_data::~Server_data() {
    wait_for_atomics(m_callback_called, m_method_callback_called, m_subscribed, m_unsubscribed,
                     m_method_payload_allocate_called);
}

Server_connector_callbacks_mock& Server_data::get_callbacks() { return m_callbacks; }

Server_connector_credentials_callbacks_mock& Server_data::get_credentials_callbacks() {
    return m_credential_callbacks;
}

Enabled_server_connector& Server_data::get_connector() { return *m_connector; }

Disabled_server_connector::Uptr Server_data::disable() {
    return Enabled_server_connector::disable(std::move(m_connector));
}

void Server_data::enable(Disabled_server_connector::Uptr disabled_connector) {
    m_connector = Disabled_server_connector::enable(std::move(disabled_connector));
}

void Server_data::update_event(Event_id const& event_id, Payload const& payload) {
    m_connector->update_event(event_id, clone_payload(payload));
}

void Server_data::update_requested_event(Event_id const& event_id, Payload const& payload) {
    m_connector->update_requested_event(event_id, clone_payload(payload));
}

std::atomic<bool> const& Server_data::expect_on_event_subscription_change(
    Event_id const& event_id, Event_state const& state,
    Event_subscription_change_callback subscription_change_callback) {
    auto& atomi = state == Event_state::subscribed ? m_subscribed : m_unsubscribed;
    EXPECT_TRUE(atomi);
    atomi = false;

    auto callback = std::make_shared<Event_subscription_change_callback>(
        std::move(subscription_change_callback));
    auto call_callback = [callback](auto& sc, auto const event_id, auto const state) {
        if (!callback->empty()) {
            (*callback)(sc, event_id, state);
        }
    };

    EXPECT_CALL(m_callbacks, on_event_subscription_change(_, event_id, state))
        .WillOnce(DoAll(call_callback, Assign(&atomi, true)));
    return atomi;
}

void Server_data::expect_on_event_subscription_change_nosync(Event_id const& event_id,
                                                             Event_state const& state) {
    EXPECT_CALL(m_callbacks, on_event_subscription_change(_, event_id, state));
}

std::atomic<bool> const& Server_data::expect_update_event_request(Event_id const& event_id) {
    EXPECT_TRUE(m_callback_called);
    m_callback_called = false;
    EXPECT_CALL(m_callbacks, on_event_update_request(_, event_id))
        .WillOnce(Assign(&m_callback_called, true));
    return m_callback_called;
}

std::atomic<bool> const& Server_data::expect_update_event_requests(Event_id const& event_id) {
    EXPECT_TRUE(m_callback_called);
    m_callback_called = false;
    EXPECT_CALL(m_callbacks, on_event_update_request(_, event_id))
        .WillOnce(Assign(&m_callback_called, true))
        .WillRepeatedly(ignore_call());
    return m_callback_called;
}

void Server_data::expect_and_respond_update_event_request(Event_id const& event_id,
                                                          Payload const& payload) {
    auto cloned = std::make_shared<Payload>(clone_payload(payload));
    EXPECT_CALL(m_callbacks, on_event_update_request(_, event_id))
        .WillOnce([cloned](Enabled_server_connector& connector, Event_id const& eid) {
            connector.update_requested_event(eid, clone_payload(*cloned));
        });
}

std::atomic<bool> const& Server_data::expect_method_allocate_payload(
    Method_id const& method_id, score::Result<Writable_payload> result) {
    EXPECT_CALL(m_callbacks, on_method_call_payload_allocate(_, method_id))
        .WillOnce(DoAll(Assign(&m_method_payload_allocate_called, true),
                        Return(ByMove(std::move(result)))));

    m_method_payload_allocate_called = false;
    return m_method_payload_allocate_called;
}

std::atomic<bool> const& Server_data::expect_and_respond_method_calls(size_t const counter,
                                                                      Method_id const& method_id,
                                                                      Payload const& payload,
                                                                      Method_result const& result) {
    EXPECT_TRUE(m_method_callback_called);
    m_method_callback_called = false;
    m_num_method_calls = 0;

    auto const reply = [this, &result, counter](auto& /*connector*/, auto /*mid*/,
                                                auto const& /*pl*/, auto const& cb) {
        if (cb) {
            cb->reply(result);
        }
        m_num_method_calls++;
        if (counter == m_num_method_calls) {
            m_method_callback_called = true;
        }
        return std::make_unique<Method_invocation>();
    };

    EXPECT_CALL(m_callbacks, on_method_call(_, method_id, payload_eq(payload), _))
        .Times(counter)
        .WillRepeatedly(reply);
    return m_method_callback_called;
}

std::atomic<bool> const& Server_data::expect_and_respond_method_call(Method_id const& method_id,
                                                                     Payload const& payload,
                                                                     Method_result const& result) {
    return expect_and_respond_method_calls(1, method_id, payload, result);
}

std::future<Method_call_reply_data_opt> Server_data::expect_and_return_method_call(
    Method_id const& method_id, Payload const& payload) {
    EXPECT_TRUE(m_method_callback_called);
    m_method_callback_called = false;

    auto saved_callback = std::make_shared<std::promise<Method_call_reply_data_opt>>();

    EXPECT_CALL(m_callbacks, on_method_call(_, method_id, payload_eq(payload), _))
        .WillOnce(
            [this, saved_callback](auto& /*connector*/, auto /*mid*/, auto const& /*pl*/, auto cb) {
                m_method_callback_called = true;
                saved_callback->set_value(std::move(cb));
                return std::make_unique<Method_invocation>();
            });

    return saved_callback->get_future();
}

std::future<void> Server_data::expect_method_calls(std::size_t const& min_num,
                                                   Method_id const& method_id,
                                                   Payload const& payload) {
    std::promise<void> methods_called;
    auto methods_called_future = methods_called.get_future();

    auto check_update_count =
        create_check_update_count(m_num_method_calls, min_num, std::move(methods_called));

    EXPECT_CALL(m_callbacks, on_method_call(_, method_id, payload_eq(payload), _))
        .WillRepeatedly(DoAll(check_update_count, InvokeWithoutArgs([]() { return nullptr; })));
    return methods_called_future;
}

Event_mode Server_data::get_event_mode(Event_id server_id) const {
    auto result = m_connector->get_event_mode(server_id);
    EXPECT_TRUE(result);
    return std::move(result).value_or(Event_mode::update);
}

void Server_data::expect_event_subscription(Event_id const& event_id) {
    expect_on_event_subscription_change_nosync(event_id, Event_state::subscribed);
    expect_on_event_subscription_change_nosync(event_id, Event_state::unsubscribed);
}

}  // namespace score::socom
