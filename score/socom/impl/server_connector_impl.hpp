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

#ifndef SRC_SOCOM_SRC_SERVER_CONNECTOR_IMPL
#define SRC_SOCOM_SRC_SERVER_CONNECTOR_IMPL

#include <future>
#include <mutex>
#include <optional>
#include <vector>

#include "endpoint.hpp"
#include "messages.hpp"
#include "runtime_registration.hpp"
#include "score/socom/final_action.hpp"
#include "score/socom/server_connector.hpp"
#include "score/socom/service_interface_definition.hpp"
#include "score/socom/service_interface_identifier.hpp"
#include "temporary_thread_id_add.hpp"

namespace score {
namespace socom {

class Runtime_impl;

namespace server_connector {

class Impl;

class Client_connection {
   public:
    explicit Client_connection(Impl& impl, Client_connector_endpoint client);

    template <typename MessageType>
    typename MessageType::Return_type receive(MessageType message) const;

    Client_connector_endpoint get_client_endpoint() const;

   private:
    Impl& m_impl;
    Client_connector_endpoint m_client;
};

class Event {
   public:
    void set_client(Client_connection const& client) { m_client = &client; }

    bool clear() {
        auto const had_client = (nullptr != m_client);
        m_client = nullptr;
        return had_client;
    }

    std::optional<Client_connector_endpoint> get_client() const {
        if (nullptr == m_client) {
            return std::nullopt;
        }
        return m_client->get_client_endpoint();
    }

   private:
    Client_connection const* m_client = nullptr;
};

class Impl final : virtual public Disabled_server_connector,
                   virtual public Enabled_server_connector {
   public:
    using Listen_endpoint = Server_connector_listen_endpoint;
    using Endpoint = Server_connector_endpoint;

    Impl(Runtime_impl& runtime, Server_service_interface_definition configuration,
         Service_instance instance, Disabled_server_connector::Callbacks callbacks,
         Final_action final_action, Posix_credentials const& credentials);
    Impl(Impl const&) = delete;
    Impl(Impl&&) = delete;
    Impl& operator=(Impl const&) = delete;
    Impl& operator=(Impl&&) = delete;

    ~Impl() noexcept override;

    // interface ::score::socom::Enabled_server_connector
    Result<Blank> update_event(Event_id server_id, Payload payload) noexcept override;
    Result<Blank> update_requested_event(Event_id server_id, Payload payload) noexcept override;
    Result<Event_mode> get_event_mode(Event_id server_id) const noexcept override;
    Impl* enable() override;
    Impl* disable() noexcept override;
    Result<Writable_payload> allocate_event_payload(Event_id event_id) noexcept override;
    Server_service_interface_definition const& get_configuration() const noexcept override;
    Service_instance const& get_service_instance() const noexcept override;

    // Endpoint APIs
    // Listen endpoint
    message::Connect::Return_type receive(message::Connect message);

    // Connection endpoint
    message::Call_method::Return_type receive(Client_connection const& client,
                                              message::Call_method message);
    message::Posix_credentials::Return_type receive(Client_connection const& client,
                                                    message::Posix_credentials const& message);
    message::Subscribe_event::Return_type receive(Client_connection const& client,
                                                  message::Subscribe_event message);
    message::Unsubscribe_event::Return_type receive(Client_connection const& client,
                                                    message::Unsubscribe_event message);
    message::Request_event_update::Return_type receive(Client_connection const& client,
                                                       message::Request_event_update message);
    message::Allocate_method_call_payload::Return_type receive(
        Client_connection const& client, message::Allocate_method_call_payload message);

   private:
    struct Event_info {
        Event_mode mode;
    };

    using Events = std::vector<Event>;
    using Event_infos = std::vector<Event_info>;

    void unsubscribe_event();
    void unsubscribe_event(Client_connection const& client);
    void unsubscribe_event(Client_connection const& client, Event_id id);
    void remove_client();

    template <typename MessageType>
    void send_all(MessageType message) const;

    template <typename MessageType>
    static void send(Client_connector_endpoint const& client, MessageType message);

    template <typename MessageType>
    static void send(std::optional<Client_connector_endpoint> const& client, MessageType message);

    template <typename MessageType>
    static typename MessageType::Return_type send(
        std::optional<Client_connector_endpoint> const& client, MessageType message,
        typename MessageType::Return_type default_return_value = {});

    Runtime_impl& m_runtime;
    Server_service_interface_definition const m_configuration;
    Service_instance const m_instance;
    Disabled_server_connector::Callbacks const m_callbacks;
#ifdef WITH_SOCOM_DEADLOCK_DETECTION
    Deadlock_detector m_deadlock_detector;
#endif
    mutable std::mutex m_mutex;
    std::promise<void> m_stop_complete_promise;
    std::promise<void> m_all_clients_disconnected_promise;
    Reference_token m_stop_block_token;                      // Protected by m_mutex
    Reference_token m_all_clients_disconnected_block_token;  // Protected by m_mutex
    Events m_subscriber;                                     // Entries protected by m_mutex
    Events m_update_requester;                               // Entries protected by m_mutex
    Event_infos m_event_infos;                               // Entries protected by m_mutex
    std::optional<Client_connection> m_client;               // Protected by m_mutex
    Registration m_registration;
    Final_action m_final_action;
    Posix_credentials m_credentials;
};

template <typename MessageType>
void Impl::send_all(MessageType message) const {
    std::unique_lock<std::mutex> lock{m_mutex};
    auto locked_client = m_client;
    lock.unlock();

    if (locked_client) {
        locked_client->get_client_endpoint().send(std::move(message));
    }
}

template <typename MessageType>
void Impl::send(Client_connector_endpoint const& client, MessageType message) {
    client.send(std::move(message));
}

template <typename MessageType>
void Impl::send(std::optional<Client_connector_endpoint> const& client, MessageType message) {
    if (client) {
        client->send(std::move(message));
    }
}

template <typename MessageType>
typename MessageType::Return_type Impl::send(
    std::optional<Client_connector_endpoint> const& client, MessageType message,
    typename MessageType::Return_type default_return_value) {
    if (client) {
        return client->send(std::move(message));
    }
    return default_return_value;
}

template <typename MessageType>
typename MessageType::Return_type Client_connection::receive(MessageType message) const {
    return m_impl.receive(*this, std::move(message));
}

inline Client_connector_endpoint Client_connection::get_client_endpoint() const { return m_client; }

}  // namespace server_connector
}  // namespace socom
}  // namespace score

#endif  // SRC_SOCOM_SRC_SERVER_CONNECTOR_IMPL
