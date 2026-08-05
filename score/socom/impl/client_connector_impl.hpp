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

#ifndef SCORE_SOCOM_CLIENT_CONNECTOR_IMPL_HPP
#define SCORE_SOCOM_CLIENT_CONNECTOR_IMPL_HPP

#include <functional>
#include <future>
#include <mutex>
#include <optional>

#include "endpoint.hpp"
#include "messages.hpp"
#include "runtime_registration.hpp"
#include "score/socom/client_connector.hpp"
#include "score/socom/service_interface_identifier.hpp"
#include "temporary_thread_id_add.hpp"

namespace score {
namespace socom {

class Runtime_impl;

namespace client_connector {
// deadlock detection.
class Impl final : public Client_connector {
   public:
    using Endpoint = Client_connector_endpoint;

    using Server_indication =
        std::function<void(::score::socom::Server_connector_listen_endpoint const&)>;

    Impl(Service_interface_definition configuration, Service_instance instance,
         Client_connector::Callbacks callbacks, Posix_credentials const& credentials);
    Impl(Impl const&) = delete;
    Impl(Impl&&) = delete;
    Impl& operator=(Impl const&) = delete;
    Impl& operator=(Impl&&) = delete;

    ~Impl() noexcept override;

    // interface ::score::socom::Client_connector
    Result<Writable_payload> allocate_method_call_payload(Method_id method_id) noexcept override;
    message::Subscribe_event::Return_type subscribe_event(Event_id client_id,
                                                          Event_mode mode) const noexcept override;
    message::Unsubscribe_event::Return_type unsubscribe_event(
        Event_id client_id) const noexcept override;
    message::Request_event_update::Return_type request_event_update(
        Event_id client_id) const noexcept override;
    message::Call_method::Return_type call_method(
        Method_id client_id, Payload payload,
        Method_call_reply_data_opt reply_data) const noexcept override;
    Result<Posix_credentials> get_peer_credentials() const noexcept override;
    Service_interface_definition const& get_configuration() const noexcept override;
    Service_instance const& get_service_instance() const noexcept override;
    bool is_service_available() const noexcept override;

    // Endpoint API
    message::Service_state_change::Return_type receive(message::Service_state_change message);
    message::Update_event::Return_type receive(message::Update_event message);
    message::Update_requested_event::Return_type receive(message::Update_requested_event message);
    message::Allocate_event_payload::Return_type receive(message::Allocate_event_payload message);

    Server_indication make_on_server_update_callback();
    void set_registration(Registration registration);

   private:
    template <typename ReturnType, typename F>
    ReturnType lock_server(F const& on_server_locked) const;

    bool set_id_mappings_and_server(message::Connect_return const& connect_return);

    Weak_reference_token create_weak_block_token() const;

    // Endpoint APIs
    template <typename MessageType>
    typename MessageType::Return_type send(MessageType message) const;

    Service_interface_definition const m_configuration;
    Service_instance const m_instance;
    Client_connector::Callbacks const m_callbacks;
#ifdef WITH_SOCOM_DEADLOCK_DETECTION
    mutable Deadlock_detector m_deadlock_detector;
#endif
    mutable std::mutex m_mutex;
    std::promise<void> m_stop_complete_promise;
    Reference_token m_stop_block_token;                 // Protected by m_mutex
    std::optional<Server_connector_endpoint> m_server;  // Protected by m_mutex
    Registration m_registration;                        // Protected by m_mutex
    Posix_credentials m_credentials;
};

template <typename ReturnType, typename F>
ReturnType Impl::lock_server(F const& on_server_locked) const {
    std::unique_lock<std::mutex> lock{m_mutex};
    auto const locked_server = m_server;
    lock.unlock();

    if (locked_server) {
        return on_server_locked(*locked_server);
    }
    return MakeUnexpected(Error::runtime_error_service_not_available);
}

template <typename MessageType>
typename MessageType::Return_type Impl::send(MessageType message) const {
    return lock_server<typename MessageType::Return_type>(
        [&message](Server_connector_endpoint const& server) {
            return server.send(std::move(message));
        });
}

}  // namespace client_connector
}  // namespace socom
}  // namespace score

#endif  // SCORE_SOCOM_CLIENT_CONNECTOR_IMPL_HPP
