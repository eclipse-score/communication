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

#include "client_connector_impl.hpp"

#include <cassert>
#include <iostream>

#include "messages.hpp"
#include "score/socom/client_connector.hpp"
#include "score/socom/event.hpp"
#include "score/socom/final_action.hpp"
#include "server_connector_impl.hpp"

namespace score {
namespace socom {
namespace client_connector {

Impl::Impl(Service_interface_definition configuration, Service_instance instance,
           Client_connector::Callbacks callbacks, Posix_credentials const& credentials)
    : m_configuration{std::move(configuration)},
      m_instance{std::move(instance)},
      m_callbacks{std::move(callbacks)},
      m_stop_block_token{
          std::make_shared<Final_action>([this]() { m_stop_complete_promise.set_value(); })},
      m_credentials{credentials} {}

Impl::~Impl() noexcept {
    {
        {
            std::lock_guard<std::mutex> const lock{m_mutex};
            m_stop_block_token.reset();
            m_server.reset();
        }
        // triggers callback call. Actions which would be done by still active alive m_registration
        // are stopped by already reset m_stop_block_token.
        m_registration.reset();
    }
#ifdef WITH_SOCOM_DEADLOCK_DETECTION

    // death tests cannot contribute to code coverage
    auto const log_on_deadlock = [this]() {
        // destruction from within callback detected
        std::cerr << "SOCom error: A callback causes the Client_connector instance to be destroyed "
                     "by which the callback is called. This leads to a deadlock because the "
                     "destructor waits until all callbacks are done.: interface="
                  << m_configuration.interface.id << std::endl;
    };

    m_deadlock_detector.check_deadlock(log_on_deadlock);
#endif
    auto const wait_for_stop_complete = [this]() { m_stop_complete_promise.get_future().wait(); };
    Final_action const catch_promise_exceptions{wait_for_stop_complete};
}

message::Subscribe_event::Return_type Impl::subscribe_event(Event_id client_id,
                                                            Event_mode mode) const noexcept {
    return send(message::Subscribe_event{client_id, mode});
}

message::Unsubscribe_event::Return_type Impl::unsubscribe_event(Event_id client_id) const noexcept {
    return send(message::Unsubscribe_event{client_id});
}

message::Request_event_update::Return_type Impl::request_event_update(
    Event_id client_id) const noexcept {
    return send(message::Request_event_update{client_id});
}

message::Call_method::Return_type Impl::call_method(
    Method_id client_id, Payload payload, Method_call_reply_data_opt reply_data) const noexcept {
    if (reply_data) {
        reply_data->set_block_token(create_weak_block_token()
#ifdef WITH_SOCOM_DEADLOCK_DETECTION
                                        ,
                                    m_deadlock_detector
#endif
        );
    }

    return send(
        message::Call_method{client_id, std::move(payload), std::move(reply_data), m_credentials});
}

Result<Writable_payload> Impl::allocate_method_call_payload(Method_id method_id) noexcept {
    return send(message::Allocate_method_call_payload{method_id});
}

Result<Posix_credentials> Impl::get_peer_credentials() const noexcept {
    return send(message::Posix_credentials{});
}

Service_interface_definition const& Impl::get_configuration() const noexcept {
    return m_configuration;
}

Service_instance const& Impl::get_service_instance() const noexcept { return m_instance; }

bool Impl::is_service_available() const noexcept { return m_server.has_value(); }

message::Service_state_change::Return_type Impl::receive(message::Service_state_change message) {
    if (message.state == Service_state::not_available) {
        std::lock_guard<std::mutex> const lock{m_mutex};
        m_server.reset();
    }
#ifdef WITH_SOCOM_DEADLOCK_DETECTION
    Temporary_thread_id_add const tmptia{m_deadlock_detector.enter_callback()};
#endif
    m_callbacks.on_service_state_change(*this, message.state, message.configuration);
}

message::Update_event::Return_type Impl::receive(message::Update_event message) {
#ifdef WITH_SOCOM_DEADLOCK_DETECTION
    Temporary_thread_id_add const tmptia{m_deadlock_detector.enter_callback()};
#endif
    m_callbacks.on_event_update(*this, message.id, std::move(message.payload));
}

message::Update_requested_event::Return_type Impl::receive(
    message::Update_requested_event message) {
#ifdef WITH_SOCOM_DEADLOCK_DETECTION
    Temporary_thread_id_add const tmptia{m_deadlock_detector.enter_callback()};
#endif
    m_callbacks.on_event_requested_update(*this, message.id, std::move(message.payload));
}

message::Allocate_event_payload::Return_type Impl::receive(
    message::Allocate_event_payload message) {
#ifdef WITH_SOCOM_DEADLOCK_DETECTION
    Temporary_thread_id_add const tmptia{m_deadlock_detector.enter_callback()};
#endif
    return m_callbacks.on_event_payload_allocate(*this, message.id);
}

Impl::Server_indication Impl::make_on_server_update_callback() {
    return [this, weak_stop_token = create_weak_block_token()](
               Server_connector_listen_endpoint const& listen_endpoint) {
        auto const locked_token = weak_stop_token.lock();
        // Destroying client-connector before this callback runs is not possible with
        // deterministic results.

        if (nullptr == locked_token) {
            // Client_connector destruction detected
            return;
        }

        auto endpoint = Client_connector_endpoint(*this, locked_token);
        auto const connect_return = listen_endpoint.send(message::Connect{endpoint});
        // Endpoint not accessible for testing to inject determinstic error-condition

        if (!connect_return) {
            return;
        }

        // As the false condition happens on the non deterministic behavior of thread scheduling
        // it cannot be tested reliably in unit tests and is therefore excluded in the coverage.

        if (set_id_mappings_and_server(*connect_return)) {
            receive(connect_return->service_state);
        }
    };
}

void Impl::set_registration(Registration registration) {
    m_registration = std::move(registration);
    assert(m_registration);
}

bool Impl::set_id_mappings_and_server(message::Connect_return const& connect_return) {
    std::lock_guard<std::mutex> const lock{m_mutex};
    // The dtor could have been started by another thread while this callback is active.
    // If the dtor is active setting m_server will lead to a deadlock. Thus check if the
    // dtor is running by checking m_stop_block_token for nullptr.
    // As this happens on the non deterministic behavior of thread scheduling it cannot
    // be tested reliably in unit tests and is therefore excluded in the coverage.
    // Only Bullseye is excluded as it may happen that the following condition is never
    // false.

    if (nullptr == m_stop_block_token) {
        return false;
    }

    m_server = connect_return.endpoint;
    return true;
}

Weak_reference_token Impl::create_weak_block_token() const {
    std::lock_guard<std::mutex> const lock{m_mutex};
    return Weak_reference_token{m_stop_block_token};
}

}  // namespace client_connector

}  // namespace socom
}  // namespace score
