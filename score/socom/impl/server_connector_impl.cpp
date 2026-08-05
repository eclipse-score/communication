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

#include "server_connector_impl.hpp"

#include <cassert>
#include <iostream>
#include <memory>
#include <mutex>

#include "messages.hpp"
#include "runtime_impl.hpp"
#include "score/socom/event.hpp"
#include "score/socom/server_connector.hpp"
#include "score/socom/service_interface_definition.hpp"
#include "score/socom/service_interface_identifier.hpp"
#include "temporary_thread_id_add.hpp"

namespace score {
namespace socom {

namespace server_connector {

Client_connection::Client_connection(Impl& impl, Client_connector_endpoint client)
    : m_impl{impl}, m_client{std::move(client)} {}

Impl::Impl(Runtime_impl& runtime, Server_service_interface_definition configuration,
           Service_instance instance, Disabled_server_connector::Callbacks callbacks,
           Final_action final_action, Posix_credentials const& credentials)
    : m_runtime{runtime},
      m_configuration{std::move(configuration)},
      m_instance{std::move(instance)},
      m_callbacks{std::move(callbacks)},
      m_subscriber(m_configuration.get_num_events()),
      m_update_requester(m_configuration.get_num_events()),
      m_event_infos(m_configuration.get_num_events()),
      m_final_action{std::move(final_action)},
      m_credentials{credentials} {
    assert(m_subscriber.size() == m_configuration.get_num_events());
    assert(m_update_requester.size() == m_configuration.get_num_events());
    assert(m_event_infos.size() == m_configuration.get_num_events());
}

Impl::~Impl() noexcept { disable(); }

Impl* Impl::enable() {
    // Defensive programming. True condition can not accour. At construction
    // m_registration == nullptr. Impl::enable function is called by
    // Disabled_server_connector::enable. After that m_registration points to a valid object and
    // Impl is moved to Enabled_server_connector which has only a disable function. This means
    // Impl::enable can not be called with a valid m_registration.

    if (m_registration != nullptr) {
        return nullptr;
    }

    m_stop_complete_promise = {};
    m_all_clients_disconnected_promise = {};
    m_stop_block_token =
        std::make_shared<Final_action>([this]() { m_stop_complete_promise.set_value(); });
    m_all_clients_disconnected_block_token = std::make_shared<Final_action>(
        [this]() { m_all_clients_disconnected_promise.set_value(); });
    m_registration = m_runtime.register_connector(m_configuration.get_interface(), m_instance,
                                                  Listen_endpoint{*this, m_stop_block_token});

    assert(m_registration != nullptr);

    return this;
}

Impl* Impl::disable() noexcept {
    if (m_registration != nullptr) {
        // this will cause callbacks being called
        // m_registration is set in enable(), which cannot be called concurrently because
        // disable() and enable() convert the type at socom-API level.
        m_registration.reset();
        {
            std::lock_guard<std::mutex> const lock{m_mutex};
            m_stop_block_token.reset();
            m_all_clients_disconnected_block_token.reset();
            unsubscribe_event();
        }
#ifdef WITH_SOCOM_DEADLOCK_DETECTION

        // death tests cannot contribute to code coverage
        auto const log_on_deadlock = [this]() {
            std::cerr << "SOCom error: A callback causes the Enabled_server_connector instance to "
                         "be destroyed by which the callback is called. This leads to a deadlock "
                         "because the destructor waits until all callbacks are done.: interface="
                      << m_configuration.get_interface().id << "instance=" << m_instance.id
                      << std::endl;
        };

        m_deadlock_detector.check_deadlock(log_on_deadlock);
#endif

        m_stop_complete_promise.get_future().wait();
        // must ensure that all callbacks are done, before calling this, otherwise we get state
        // change races
        send_all(message::Service_state_change{Service_state::not_available, m_configuration});
        m_all_clients_disconnected_promise.get_future().wait();
    }
    assert(m_registration == nullptr);
    return this;
}

Result<Writable_payload> Impl::allocate_event_payload(Event_id event_id) noexcept {
    if (event_id >= m_configuration.get_num_events()) {
        return MakeUnexpected(Server_connector_error::logic_error_id_out_of_range);
    }

    assert(event_id < m_subscriber.size());

    std::unique_lock<std::mutex> lock{m_mutex};
    // May throw std::bad_alloc: left unhandled as a design decision
    auto const clients = m_subscriber[event_id].get_client();
    lock.unlock();

    return send(
        clients, message::Allocate_event_payload{event_id},
        MakeUnexpected(Server_connector_error::runtime_error_no_client_subscribed_for_event));
}

Server_service_interface_definition const& Impl::get_configuration() const noexcept {
    return m_configuration;
}

Service_instance const& Impl::get_service_instance() const noexcept { return m_instance; }

Result<Blank> Impl::update_event(Event_id server_id, Payload payload) noexcept {
    if (server_id >= m_configuration.get_num_events()) {
        return MakeUnexpected(Server_connector_error::logic_error_id_out_of_range);
    }

    assert(server_id < m_subscriber.size());

    std::unique_lock<std::mutex> lock{m_mutex};
    // May throw std::bad_alloc: left unhandled as a design decision
    auto const clients = m_subscriber[server_id].get_client();
    lock.unlock();

    // May throw std::bad_alloc: left unhandled as a design decision
    send(clients, message::Update_event{server_id, std::move(payload)});
    return Result<Blank>{};
}

Result<Blank> Impl::update_requested_event(Event_id server_id, Payload payload) noexcept {
    if (server_id >= m_configuration.get_num_events()) {
        return MakeUnexpected(Server_connector_error::logic_error_id_out_of_range);
    }

    assert(server_id < m_update_requester.size());

    std::unique_lock<std::mutex> lock{m_mutex};
    // May throw std::bad_alloc: left unhandled as a design decision
    auto const clients = m_update_requester[server_id].get_client();
    m_update_requester[server_id].clear();
    lock.unlock();

    // May throw std::bad_alloc: left unhandled as a design decision
    send(clients, message::Update_requested_event{server_id, std::move(payload)});
    return Result<Blank>{};
}

Result<Event_mode> Impl::get_event_mode(Event_id server_id) const noexcept {
    if (server_id >= m_configuration.get_num_events()) {
        return MakeUnexpected(Server_connector_error::logic_error_id_out_of_range);
    }

    assert(server_id < m_event_infos.size());

    std::lock_guard<std::mutex> const lock{m_mutex};
    return m_event_infos[server_id].mode;
}

void Impl::unsubscribe_event() {
    for (std::size_t id = 0U; id < m_configuration.get_num_events(); ++id) {
        m_subscriber[id].clear();
        m_update_requester[id].clear();
    }
}

void Impl::unsubscribe_event(Client_connection const& client) {
    for (std::size_t id = 0U; id < m_configuration.get_num_events(); ++id) {
        unsubscribe_event(client, static_cast<Event_id>(id));
    }
}

void Impl::unsubscribe_event(Client_connection const& /* client */, Event_id id) {
    assert(id < m_subscriber.size());
    assert(id < m_update_requester.size());
    assert(id < m_event_infos.size());

    std::unique_lock<std::mutex> lock{m_mutex};
    auto const was_subscribed = m_subscriber[id].clear();
    (void)m_update_requester[id].clear();

    lock.unlock();

    if (was_subscribed) {
#ifdef WITH_SOCOM_DEADLOCK_DETECTION
        Temporary_thread_id_add const tmptia{m_deadlock_detector.enter_callback()};
#endif
        m_callbacks.on_event_subscription_change(*this, id, Event_state::unsubscribed);
    }
}

void Impl::remove_client() {
    unsubscribe_event(*m_client);
    std::unique_lock<std::mutex> lock{m_mutex};
    m_client.reset();
    // let removed_client get out of scope after unlock (destruction)
    lock.unlock();
}

message::Connect::Return_type Impl::receive(message::Connect message) {
    std::unique_lock<std::mutex> lock{m_mutex};
    // Destroying server-connector before receiving is not possible with deterministic results.

    if (nullptr == m_stop_block_token) {
        return MakeUnexpected(Error::runtime_error_service_not_available);
    }

    // should have been checked already in Runtime
    assert(!m_client.has_value());

    m_client.emplace(*this, message.endpoint);
    auto stop_block_token_copy = m_all_clients_disconnected_block_token;
    lock.unlock();

    auto reference_token = std::make_shared<Final_action>(
        [this, stop_block_token_copy = std::move(stop_block_token_copy)]() {
            this->remove_client();
        });

    return message::Connect::Return_type{
        {Server_connector_endpoint{*m_client, std::move(reference_token)},
         message::Service_state_change{Service_state::available, m_configuration}}};
}

message::Call_method::Return_type Impl::receive(Client_connection const& /*client*/,
                                                message::Call_method message) {
    if (message.id >= m_configuration.get_num_methods()) {
        return MakeUnexpected(Error::logic_error_id_out_of_range);
    }

    assert(message.id < m_configuration.get_num_methods());

#ifdef WITH_SOCOM_DEADLOCK_DETECTION
    Temporary_thread_id_add const tmptia{m_deadlock_detector.enter_callback()};
#endif
    return message::Call_method::Return_type(
        m_callbacks.on_method_call(*this, message.id, std::move(message.payload),
                                   std::move(message.reply_data), message.credentials));
}

message::Allocate_method_call_payload::Return_type Impl::receive(
    Client_connection const& /*client*/, message::Allocate_method_call_payload message) {
    if (message.id >= m_configuration.get_num_methods()) {
        return MakeUnexpected(Error::logic_error_id_out_of_range);
    }

    assert(message.id < m_configuration.get_num_methods());

#ifdef WITH_SOCOM_DEADLOCK_DETECTION
    Temporary_thread_id_add const tmptia{m_deadlock_detector.enter_callback()};
#endif
    return m_callbacks.on_method_call_payload_allocate(*this, message.id);
}

message::Posix_credentials::Return_type Impl::receive(
    Client_connection const& /*client*/, message::Posix_credentials const& /* message */) {
    return message::Posix_credentials::Return_type(m_credentials);
}

message::Subscribe_event::Return_type Impl::receive(Client_connection const& client,
                                                    message::Subscribe_event message) {
    if (message.id >= m_event_infos.size()) {
        return MakeUnexpected(Error::logic_error_id_out_of_range);
    }

    assert(message.id < m_event_infos.size());
    assert(message.id < m_subscriber.size());
    assert(message.id < m_update_requester.size());

    std::unique_lock<std::mutex> lock{m_mutex};

    auto const already_subscribed = m_subscriber[message.id].get_client().has_value();
    auto const already_update_requester = m_update_requester[message.id].get_client().has_value();
    m_subscriber[message.id].set_client(client);
    auto const is_update_requester = message.mode == Event_mode::update_and_initial_value;

    if (is_update_requester) {
        m_update_requester[message.id].set_client(client);
        m_event_infos[message.id].mode = Event_mode::update_and_initial_value;
    }

    lock.unlock();

    if (!already_subscribed) {
#ifdef WITH_SOCOM_DEADLOCK_DETECTION
        Temporary_thread_id_add const tmptia{m_deadlock_detector.enter_callback()};
#endif
        m_callbacks.on_event_subscription_change(*this, message.id, Event_state::subscribed);
    }

    if (is_update_requester && !already_update_requester) {
#ifdef WITH_SOCOM_DEADLOCK_DETECTION
        Temporary_thread_id_add const tmptia{m_deadlock_detector.enter_callback()};
#endif
        m_callbacks.on_event_update_request(*this, message.id);
    }

    return message::Subscribe_event::Return_type{};
}

message::Unsubscribe_event::Return_type Impl::receive(Client_connection const& client,
                                                      message::Unsubscribe_event message) {
    if (message.id >= m_event_infos.size()) {
        return MakeUnexpected(Error::logic_error_id_out_of_range);
    }

    unsubscribe_event(client, message.id);
    return message::Unsubscribe_event::Return_type{};
}

message::Request_event_update::Return_type Impl::receive(Client_connection const& client,
                                                         message::Request_event_update message) {
    if (message.id >= m_update_requester.size()) {
        return MakeUnexpected(Error::logic_error_id_out_of_range);
    }

    assert(message.id < m_update_requester.size());

    std::unique_lock<std::mutex> lock{m_mutex};

    auto const already_update_requester = m_update_requester[message.id].get_client().has_value();
    if (already_update_requester) {
        return message::Request_event_update::Return_type{};
    }

    m_update_requester[message.id].set_client(client);
    lock.unlock();

    {
#ifdef WITH_SOCOM_DEADLOCK_DETECTION
        Temporary_thread_id_add const tmptia{m_deadlock_detector.enter_callback()};
#endif
        m_callbacks.on_event_update_request(*this, message.id);
    }
    return message::Request_event_update::Return_type{};
}

}  // namespace server_connector

std::unique_ptr<Enabled_server_connector> Disabled_server_connector::enable(
    std::unique_ptr<Disabled_server_connector> connector) {
    std::unique_ptr<Enabled_server_connector> result;
    auto* enabled_server_connector = connector->enable();
    assert(enabled_server_connector != nullptr);
    // clang-format off
    // NOLINTNEXTLINE(bugprone-unused-return-value): pointer is already stored in enabled_server_connector
    connector.release();
    // clang-format on
    result.reset(enabled_server_connector);
    return result;
}

std::unique_ptr<Disabled_server_connector> Enabled_server_connector::disable(
    std::unique_ptr<Enabled_server_connector> connector) noexcept {
    std::unique_ptr<Disabled_server_connector> result;
    auto* disabled_server_connector = connector->disable();
    assert(disabled_server_connector != nullptr);
    // clang-format off
    // NOLINTNEXTLINE(bugprone-unused-return-value): pointer is already stored in disabled_server_connector
    connector.release();
    // clang-format on
    result.reset(disabled_server_connector);
    return result;
}

}  // namespace socom
}  // namespace score
