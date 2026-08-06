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

namespace score::socom
{

class Runtime_impl;

namespace server_connector
{

class Impl;

class Client_connection
{
  public:
    explicit Client_connection(Impl& impl, Client_connector_endpoint client);

    template <typename MessageType>
    typename MessageType::Return_type receive(MessageType message) const;

    Client_connector_endpoint get_client_endpoint() const;

  private:
    Impl& m_impl;
    Client_connector_endpoint m_client;
};

class Event
{
  public:
    void set_client(const Client_connection& client)
    {
        m_client = &client;
    }

    bool clear()
    {
        const auto had_client = (nullptr != m_client);
        m_client = nullptr;
        return had_client;
    }

    std::optional<Client_connector_endpoint> get_client() const
    {
        if (nullptr == m_client)
        {
            return std::nullopt;
        }
        return m_client->get_client_endpoint();
    }

  private:
    const Client_connection* m_client = nullptr;
};

class Impl final : virtual public Disabled_server_connector, virtual public Enabled_server_connector
{
  public:
    using Listen_endpoint = Server_connector_listen_endpoint;
    using Endpoint = Server_connector_endpoint;

    Impl(Runtime_impl& runtime,
         Server_service_interface_definition configuration,
         Service_instance instance,
         Disabled_server_connector::Callbacks callbacks,
         Final_action final_action,
         const Posix_credentials& credentials);
    Impl(const Impl&) = delete;
    Impl(Impl&&) = delete;
    Impl& operator=(const Impl&) = delete;
    Impl& operator=(Impl&&) = delete;

    ~Impl() noexcept override;

    // interface ::score::socom::Enabled_server_connector
    Result<void> update_event(Event_id server_id, Payload payload) noexcept override;
    Result<void> update_requested_event(Event_id server_id, Payload payload) noexcept override;
    Result<Event_mode> get_event_mode(Event_id server_id) const noexcept override;
    Impl* enable() override;
    Impl* disable() noexcept override;
    Result<Writable_payload> allocate_event_payload(Event_id event_id) noexcept override;
    const Server_service_interface_definition& get_configuration() const noexcept override;
    const Service_instance& get_service_instance() const noexcept override;

    // Endpoint APIs
    // Listen endpoint
    message::Connect::Return_type receive(message::Connect message);

    // Connection endpoint
    message::Call_method::Return_type receive(const Client_connection& client, message::Call_method message);
    message::Posix_credentials::Return_type receive(const Client_connection& client,
                                                    const message::Posix_credentials& message);
    message::Subscribe_event::Return_type receive(const Client_connection& client, message::Subscribe_event message);
    message::Unsubscribe_event::Return_type receive(const Client_connection& client,
                                                    message::Unsubscribe_event message);
    message::Request_event_update::Return_type receive(const Client_connection& client,
                                                       message::Request_event_update message);
    message::Allocate_method_call_payload::Return_type receive(const Client_connection& client,
                                                               message::Allocate_method_call_payload message);

  private:
    struct Event_info
    {
        Event_mode mode;
    };

    using Events = std::vector<Event>;
    using Event_infos = std::vector<Event_info>;

    void unsubscribe_event();
    void unsubscribe_event(const Client_connection& client);
    void unsubscribe_event(const Client_connection& client, Event_id id);
    void remove_client();

    template <typename MessageType>
    void send_all(MessageType message) const;

    template <typename MessageType>
    static void send(const Client_connector_endpoint& client, MessageType message);

    template <typename MessageType>
    static void send(const std::optional<Client_connector_endpoint>& client, MessageType message);

    template <typename MessageType>
    static typename MessageType::Return_type send(const std::optional<Client_connector_endpoint>& client,
                                                  MessageType message,
                                                  typename MessageType::Return_type default_return_value = {});

    Runtime_impl& m_runtime;
    const Server_service_interface_definition m_configuration;
    const Service_instance m_instance;
    const Disabled_server_connector::Callbacks m_callbacks;
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
void Impl::send_all(MessageType message) const
{
    std::unique_lock<std::mutex> lock{m_mutex};
    auto locked_client = m_client;
    lock.unlock();

    if (locked_client)
    {
        locked_client->get_client_endpoint().send(std::move(message));
    }
}

template <typename MessageType>
void Impl::send(const Client_connector_endpoint& client, MessageType message)
{
    client.send(std::move(message));
}

template <typename MessageType>
void Impl::send(const std::optional<Client_connector_endpoint>& client, MessageType message)
{
    if (client)
    {
        client->send(std::move(message));
    }
}

template <typename MessageType>
typename MessageType::Return_type Impl::send(const std::optional<Client_connector_endpoint>& client,
                                             MessageType message,
                                             typename MessageType::Return_type default_return_value)
{
    if (client)
    {
        return client->send(std::move(message));
    }
    return default_return_value;
}

template <typename MessageType>
typename MessageType::Return_type Client_connection::receive(MessageType message) const
{
    return m_impl.receive(*this, std::move(message));
}

inline Client_connector_endpoint Client_connection::get_client_endpoint() const
{
    return m_client;
}

}  // namespace server_connector
}  // namespace score::socom

#endif  // SRC_SOCOM_SRC_SERVER_CONNECTOR_IMPL
