/********************************************************************************
 * Copyright (C) 2017-2026 LG Electronics Inc.
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 ********************************************************************************/

#ifndef LG_SOMEIP_PACKET_ROUTER_PROXY_H
#define LG_SOMEIP_PACKET_ROUTER_PROXY_H

#include <cstdint>
#include <memory>
#include <type_traits>

#include <endpoint/Endpoint.h>
#include <multiplex/Multiplexer.h>
#include <runtime/ApplicationManager.h>
#include <packetrouter/PacketRouterCommonType.h>
#include <packetrouter/ApplicationRouter.h>
#include <utils/time/TimerManager.h>

namespace lgsomeip {

class ApplicationManager;

class PacketRouterProxy : public ApplicationRouter
#if defined(ENABLE_QNX_MESSAGE_PASSING)
    ,
                          public EndPointMessagePassingListener
#endif // ENABLE_QNX_MESSAGE_PASSING
{
public:
    PacketRouterProxy(ApplicationManager* host);

    void init() override;
    void start() override;
    void stop() override;
    void join() override;

    void on_connect(std::shared_ptr<Endpoint> server_endpoint, std::shared_ptr<Endpoint> client_endpoint);
    void on_message(std::shared_ptr<Endpoint> endpoint, std::uint8_t* message, std::size_t message_length);
    void on_disconnect(std::shared_ptr<Endpoint> endpoint);
    void on_reconnect();
#if defined(ENABLE_SOMEIP_IPC)
    void add_ipc_route(std::uint16_t service_id, std::uint16_t instance_id, std::uint16_t event_group_id,
                       std::uint16_t app_id, bool is_provider);
    void set_ipc_connection_info(std::uint16_t service_id, std::uint16_t instance_id, std::uint16_t event_group_id,
                                 bool is_provider, std::shared_ptr<Endpoint> ipc_sender);
    void set_ipc_connection_info_event_group_id(std::uint16_t service_id, std::uint16_t instance_id,
                                                std::uint16_t event_group_id, bool is_provider,
                                                std::shared_ptr<Endpoint> ipc_sender);
    void set_ipc_connection_info_instance_id(std::uint16_t service_id, std::uint16_t instance_id,
                                             std::uint16_t event_group_id, bool is_provider,
                                             std::shared_ptr<Endpoint> ipc_sender);
    void set_ipc_connection_info_service_id(std::uint16_t service_id, std::uint16_t instance_id,
                                            std::uint16_t event_group_id, bool is_provider,
                                            std::shared_ptr<Endpoint> ipc_sender);
    bool find_ipc_connection_info_sender(std::uint16_t service_id, std::uint16_t instance_id,
                                         std::uint16_t event_group_id, bool is_provider,
                                         std::shared_ptr<Endpoint> ipc_sender);
    bool find_ipc_connection_info_event_group_id(std::uint16_t service_id, std::uint16_t instance_id,
                                                 std::uint16_t event_group_id);
    bool find_ipc_connection_info_instance_id(std::uint16_t service_id, std::uint16_t instance_id);
    bool find_ipc_connection_info_service_id(std::uint16_t service_id);
#endif // ENABLE_SOMEIP_IPC

    virtual void start_retry_connect();
    virtual void stop_retry_connect();
    virtual void do_connect();
#if defined(ENABLE_QNX_MESSAGE_PASSING)
#if !defined(ENABLE_SOMEIP_IPC)
    virtual void on_message_passing_timer(const int32_t id) override;
    virtual void on_message_passing_connect(std::shared_ptr<Endpoint> server_endpoint,
                                            std::shared_ptr<Endpoint> client_endpoint);
    virtual void on_message_passing_disconnect(std::shared_ptr<Endpoint> endpoint);
#else
    virtual void on_message_passing_connect(std::shared_ptr<Endpoint> server_endpoint,
                                            std::shared_ptr<Endpoint> client_endpoint, int connection_id);
    virtual void on_message_passing_disconnect(std::shared_ptr<Endpoint> endpoint, int connection_id);
#endif // ENABLE_SOMEIP_IPC
    virtual void on_message_passing_message(std::shared_ptr<Endpoint> endpoint, std::uint8_t* message,
                                            std::size_t message_length);
#endif // ENABLE_QNX_MESSAGE_PASSING

    std::shared_ptr<lgsomeip::osabstraction::Multiplexer> get_multiplexer() {
        return multiplexer_;
    }

public:
    void send_message(std::shared_ptr<MessageSD> message) override;
    void send_message(std::shared_ptr<MessageSOMEIP> message) override;
    void send_message(MessageSOMEIP& message) override;
#if defined(ENABLE_SOMEIP_IPC)
    void send_ipc_message(std::shared_ptr<MessageSOMEIP> message) override;
    void send_ipc_message(MessageSOMEIP& message, bool is_provider) override;
    void send_ipc_response_message(MessageSOMEIP& message, std::uint16_t request_id, std::uint16_t app_id) override;
    void on_internal_request(std::uint16_t service_id, std::uint16_t instance_id, std::uint16_t request_id,
                             std::uint16_t app_id) override;
#endif // ENABLE_SOMEIP_IPC

private:
    void do_register_application();
    void do_un_register_application();
    std::shared_ptr<MessageSD> compose_application_register(bool register_application);

private:
    void do_send_message(std::shared_ptr<Endpoint> endpoint, std::uint8_t* message, std::size_t message_length);

    // When multiple threads try to send messages in the PacketRouterProxy, the messages can be overwritten.
    // This mutex prevents multiple threads from sending messages simultaneously.
    std::mutex send_message_mutex_;
    std::uint8_t send_buffer_someip_sd_[SOMEIP_UDP_MAX_PAYLOAD_SIZE];

private:
    ApplicationManager* host_;
    bool running_ = false;

    std::shared_ptr<lgsomeip::osabstraction::Multiplexer> multiplexer_;
    std::shared_ptr<Endpoint> receiver_;
    std::shared_ptr<Endpoint> listener_;
    std::shared_ptr<Endpoint> sender_;
#if defined(ENABLE_SOMEIP_IPC)
    std::map<std::uint16_t, struct RoutingApplicationInfo> local_applications_;
    std::map<std::uint32_t, struct RoutingConnectionInfo> connected_endpoints_;

    // IPC Info
    // service Id, Vector<app_id>
    std::map<std::uint16_t,
             std::map<std::uint16_t, std::map<std::uint16_t, std::vector<std::pair<bool, std::shared_ptr<Endpoint>>>>>>
        ipc_connection_info_;
    std::map<std::uint16_t, struct RoutingRequestPacket> ipc_request_map_;
    std::map<std::uint16_t, std::shared_ptr<Endpoint>> ipc_senders_;
#endif // ENABLE_SOMEIP_IPC

    std::shared_ptr<Endpoint> message_passing_receiver_;
    std::shared_ptr<Endpoint> message_passing_listener_;
    std::shared_ptr<Endpoint> message_passing_sender_;
#if defined(ENABLE_SOMEIP_IPC)
    std::mutex mutex_processing_;
#endif // ENABLE_SOMEIP_IPC
};

} // namespace lgsomeip

#endif // LG_SOMEIP_PACKET_ROUTER_PROXY_H
