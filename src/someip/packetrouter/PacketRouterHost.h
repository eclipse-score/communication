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

#ifndef LG_SOMEIP_PACKET_ROUTER_HOST_H
#define LG_SOMEIP_PACKET_ROUTER_HOST_H

#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <thread>

#include <message/Message.h>
#include <multiplex/Multiplexer.h>
#include <packetrouter/PacketRouterCommonType.h>
#include <packetrouter/ServiceRouter.h>
// #include <endpoint/EndpointUtils.h>
#include <runtime/ServiceManager.h>

#include <buffer/buffer.hpp>
#include <e2exf/config.hpp>
#include <e2e/profile/profile01/profile_01.hpp>
#include <e2e/profile/profile01/protector.hpp>
#include <e2e/profile/profile01/checker.hpp>
#include <e2e/profile/profile_custom/profile_custom.hpp>
#include <e2e/profile/profile_custom/protector.hpp>
#include <e2e/profile/profile_custom/checker.hpp>

#if defined(ENABLE_SOMEIP_PACKET_FILTERING)
#include "PacketFiltering.h"
#endif

namespace lgsomeip {

class ServiceManager;

class PacketRouterHost : public ServiceRouter
#if defined(ENABLE_QNX_MESSAGE_PASSING)
    ,
                         public EndPointMessagePassingListener
#endif // ENABLE_QNX_MESSAGE_PASSING
{
public:
    PacketRouterHost(ServiceManager* host);
    ~PacketRouterHost() override;

    void init() override;
    void start() override;
    void stop() override;

    inline std::shared_ptr<lgsomeip::osabstraction::Multiplexer> get_multiplexer() override {
        return multiplexer_;
    }
    std::shared_ptr<lgsomeip::osabstraction::Address> get_address(std::uint16_t app_id);

    std::shared_ptr<Endpoint> get_service_discovery_multicast_endpoint() override {
        return service_discovery_multicast_;
    }
    std::shared_ptr<Endpoint> get_service_discovery_unicast_endpoint() override {
        return service_discovery_unicast_;
    }

    std::shared_ptr<lgsomeip::osabstraction::Address> make_address() override;
    std::shared_ptr<lgsomeip::osabstraction::Address> make_address(bool reliable, std::uint16_t port,
                                                                   std::uint8_t vlan_priority);

    void on_connect(std::shared_ptr<Endpoint> server_endpoint, std::shared_ptr<Endpoint> client_endpoint);
    void on_disconnect(std::shared_ptr<Endpoint> endpoint);
    void reboot_route(std::shared_ptr<lgsomeip::osabstraction::Address> address) override;

public:
    // Section: Multiple Service-Instances : Mapping (Service ID, Socket Addr) to Instance ID
    //  Transport protocol Binding (4.2.1)
    //  Transport Protocol Bindings / Service Instance Mapped to L4 Port
    //  Multiple Service-Instances (4.2.1.3)
    void set_instance_id(std::uint16_t service_id, std::uint16_t instance_id,
                         std::shared_ptr<lgsomeip::osabstraction::Address> addr);
    void set_instance_id(std::uint16_t service_id, std::uint16_t instance_id, std::uint16_t app_id) override;

    std::uint16_t find_instance_id(std::uint16_t service_id, std::shared_ptr<Endpoint> endpoint);
    struct RoutingServiceInfo* get_service_instance(std::uint16_t service_id, std::shared_ptr<Endpoint> endpoint);

    // Section: Message Receiver
    bool is_reboot(const std::shared_ptr<Endpoint> endpoint, bool reboot_flag, std::uint32_t request_id);
    void on_message(std::shared_ptr<Endpoint> endpoint, std::uint8_t* message, std::size_t message_length);

    // SOME/IP Data (Internal)
    void on_internal_message(std::shared_ptr<Endpoint> endpoint, std::uint8_t* message, std::size_t message_length);
    void on_internal_request(std::shared_ptr<Endpoint> endpoint, std::uint8_t* message, std::size_t message_length,
                             std::uint32_t message_id, std::uint32_t request_id, bool no_response = false);
    void on_internal_response(std::shared_ptr<Endpoint> endpoint, std::uint8_t* message, std::size_t message_length,
                              std::uint32_t message_id, std::uint32_t request_id);
    void on_internal_notification(std::shared_ptr<Endpoint> endpoint, std::uint8_t* message, std::size_t message_length,
                                  std::uint32_t message_id, std::uint32_t request_id);

    // SOME/IP Data (External)
    void on_external_message(std::shared_ptr<Endpoint> endpoint, std::uint8_t* message, std::size_t message_length);
    void on_external_request(std::shared_ptr<Endpoint> endpoint, std::uint8_t* message, std::size_t message_length,
                             std::uint32_t message_id, std::uint32_t request_id, bool no_response = false);
    void on_external_response(std::shared_ptr<Endpoint> endpoint, std::uint8_t* message, std::size_t message_length,
                              std::uint32_t message_id, std::uint32_t request_id);
    void on_external_notification(std::shared_ptr<Endpoint> endpoint, std::uint8_t* message, std::size_t message_length,
                                  std::uint32_t message_id, std::uint32_t request_id, std::uint8_t retry_count = 0);

    // Application Connection
    void on_application_control_message(std::shared_ptr<Endpoint> endpoint, std::shared_ptr<MessageSD> message);

#if defined(ENABLE_QNX_MESSAGE_PASSING)
#if !defined(ENABLE_SOMEIP_IPC)
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
    void on_message_passing_application_control_message(std::shared_ptr<Endpoint> endpoint,
                                                        std::shared_ptr<MessageSD> message);

    void message_passing_set_timer(const int32_t id, const std::uint32_t interval_milliseconds, const bool periodic,
                                   std::function<void(void)> handler) override;
    void message_passing_kill_timer(const int32_t id) override;
    void on_message_passing_timer(const int32_t id);
#endif // ENABLE_QNX_MESSAGE_PASSING

public:
    // Section: Service Control Operation
    //  Transport protocol Binding (4.2.1)
    //  UDP Binding (4.2.1.1)
    //  TCP Binding (4.2.1.2)
    bool add_route(std::uint16_t service_id, std::uint16_t instance_id, std::uint16_t app_id = 0,
                   std::shared_ptr<lgsomeip::osabstraction::Address> remote_tcp_address = nullptr,
                   std::uint16_t local_tcp_port = 0,
                   std::shared_ptr<lgsomeip::osabstraction::Address> remote_udp_address = nullptr,
                   std::uint16_t local_udp_port = 0, std::uint8_t vlan_priority = 0xff,
                   std::shared_ptr<std::vector<std::shared_ptr<lgsomeip::osabstraction::Address>>>
                       local_multicast_list = nullptr) override;
    void remove_route(std::uint16_t service_id, std::uint16_t instance_id) override;

    std::int32_t find_connection(std::uint16_t service_id, std::uint16_t instance_id,
                                 std::shared_ptr<lgsomeip::osabstraction::Address> address) override;

private:
    bool add_new_server_route(
        std::uint16_t service_id, std::uint16_t instance_id, std::uint16_t app_id,
        std::shared_ptr<lgsomeip::osabstraction::Address> local_tcp_address,
        std::shared_ptr<lgsomeip::osabstraction::Address> local_udp_address,
        std::shared_ptr<std::vector<std::shared_ptr<lgsomeip::osabstraction::Address>>> local_multicast_list,
        bool secure_connection);
    bool add_new_client_route(std::uint16_t service_id, std::uint16_t instance_id,
                              std::shared_ptr<lgsomeip::osabstraction::Address> remote_tcp_address,
                              std::shared_ptr<lgsomeip::osabstraction::Address> local_tcp_port,
                              std::shared_ptr<lgsomeip::osabstraction::Address> remote_udp_address,
                              std::shared_ptr<lgsomeip::osabstraction::Address> local_udp_port, bool secure_connection);
    std::shared_ptr<Endpoint> find_or_create_server_endpoint(std::shared_ptr<lgsomeip::osabstraction::Address> address,
                                                             bool secure_connection);
    std::shared_ptr<Endpoint>
    find_or_create_client_endpoint(std::shared_ptr<lgsomeip::osabstraction::Address> local_addr,
                                   std::shared_ptr<lgsomeip::osabstraction::Address> remote_addr,
                                   bool secure_connection);
    std::shared_ptr<Endpoint>
    find_or_create_multicast_endpoint(std::shared_ptr<lgsomeip::osabstraction::Address> address);

    void remove_route(std::uint16_t app_id);
    void remove_route(std::shared_ptr<lgsomeip::osabstraction::Address> address);

    std::shared_ptr<Endpoint>
    find_endpoint(std::shared_ptr<lgsomeip::osabstraction::Address> source_address,
                  std::shared_ptr<lgsomeip::osabstraction::Address> destination_address = nullptr);

    void wait_for_ongoing_add_route(const std::shared_ptr<lgsomeip::osabstraction::Address>& local_addr,
                                    const std::shared_ptr<lgsomeip::osabstraction::Address>& remote_addr = nullptr);
    void remove_ongoing_add_route(const std::shared_ptr<lgsomeip::osabstraction::Address>& local_addr,
                                  const std::shared_ptr<lgsomeip::osabstraction::Address>& remote_addr = nullptr);

public:
    // Section: Event Subscribe Control Operation
    void add_subscribe_route(std::uint16_t service_id, std::uint16_t instance_id, std::uint16_t event_id,
                             std::uint16_t app_id = 0,
                             std::shared_ptr<lgsomeip::osabstraction::Address> address = nullptr,
                             std::uint16_t event_group_id = 0) override;
    void remove_subscribe_route(std::uint16_t service_id, std::uint16_t instance_id, std::uint16_t event_id,
                                std::uint16_t app_id,
                                std::shared_ptr<lgsomeip::osabstraction::Address> address) override;
    void remove_subscribe_route(std::uint16_t service_id, std::uint16_t instance_id, std::uint16_t event_id,
                                std::uint16_t app_id) override;
    void remove_subscribe_route(std::uint16_t app_id);
    void remove_subscribe_route(std::shared_ptr<lgsomeip::osabstraction::Address> address);

public:
    // Section: Message Send Operation
    void send_internal_message(std::uint8_t* message, std::size_t message_length, std::uint16_t target_app_id) override;
    void send_external_sd_message(std::uint8_t* message, std::size_t message_length,
                                  std::shared_ptr<lgsomeip::osabstraction::Address> target_address = nullptr,
                                  bool multicast = true) override;
    void check_and_send_magic_cookies() override;

private:
    void send_error(std::shared_ptr<Endpoint> endpoint, std::uint8_t* message, std::uint32_t message_id,
                    std::uint8_t return_code);

private:
    // Section: Internal & External Connection Management
    void register_connected_endpoint(std::shared_ptr<Endpoint> ep, std::uint16_t app_id = 0);
    void remove_app_before_connection(const std::int32_t& file_descriptor,
                                      const RoutingConnectionInfo& connection_info);
    void remove_connected_endpoint(std::shared_ptr<Endpoint> endpoint, bool multicast = false);

    ServiceManager* host_;
    std::shared_ptr<lgsomeip::osabstraction::Multiplexer> multiplexer_; // Multiplexer

    std::shared_ptr<EndpointTCPServer<PacketRouterHost>> local_receiver_; // Local Service Socket
#if defined(ENABLE_QNX_MESSAGE_PASSING)
    std::shared_ptr<EndpointMessagePassingServer> local_message_passing_receiver_; // Endpoint for message passing
#else
    std::shared_ptr<EndpointTCPServer<PacketRouterHost>>
        local_message_passing_receiver_; // Endpoint temp(nullptr) for message passing
#endif // ENABLE_QNX_MESSAGE_PASSING

    // connection management by app-id (internal app info)
    std::map<std::uint16_t, struct RoutingApplicationInfo> local_applications_;

    // Management All Endpoint / Mapping fd to Connection Info
    std::unordered_map<std::uint32_t, struct RoutingConnectionInfo> connected_endpoints_;

    // List of on-going Connection (Processing addRoute)
    // <Local Port, Remote Address, isReliable>
    std::map<std::tuple<std::uint16_t, std::string, bool>, bool> ongoing_add_routes_;
    std::mutex add_route_mutex_;

    // Management All Instance Map
    std::map<std::uint16_t, std::map<std::uint32_t, std::uint16_t>> instance_id_map_;

    // ServiceDiscovery Connection
    std::shared_ptr<lgsomeip::osabstraction::Address> service_discovery_address_;
    std::shared_ptr<Endpoint> service_discovery_multicast_;
    std::shared_ptr<Endpoint> service_discovery_unicast_;

    // To handle the threads that are used to retry to bind SOME/IP-SD unicast socket.
    std::mutex sd_unicast_binding_mutex_;
    std::condition_variable sd_unicast_binding_condition_;
    bool sd_unicast_binding_success_{false};
    bool sd_unicast_binding_failed_{false};
    bool sd_unicast_binding_stop_{false};
    std::thread sd_unicast_binding_thread_;
    std::thread sd_unicast_start_listen_thread_;

    // E2E
    std::map<vsomeip::e2exf::data_identifier, std::shared_ptr<vsomeip::e2e::profile_interface::protector>>
        e2e_custom_protectors_;
    std::map<vsomeip::e2exf::data_identifier, std::shared_ptr<vsomeip::e2e::profile_interface::checker>>
        e2e_custom_checkers_;

private:
    // Section: Service / Message Management (Property)
    // External Socket (for External ECU) / Mappping service id to app id
    std::map<std::uint16_t, std::map<std::uint16_t, struct RoutingServiceInfo>> registered_service_info_;

    // To protect variables used in adding and removing routes such as registered_service_info_ and
    // connected_endpoints_, a mutex should be applied.
    std::recursive_mutex route_management_mutex_;

    // mapping Client ID to Endpoint
    std::map<std::uint16_t, struct RoutingRequestPacket> request_map_;
    std::map<std::uint32_t, std::uint32_t> external_to_internal_map_;

    // Reboot Session Info
    // srcAddr, dstAddr, sessionID, rebootFlag
    std::map<std::pair<std::string, std::string>, std::pair<uint16_t, bool>> reboot_info_;

    // process the trigger message to write the current state of SOME/IP services
    void process_trigger_writing_service_state(void);

#if defined(ENABLE_SOMEIP_PACKET_FILTERING)
    std::unique_ptr<PacketFiltering> packet_filtering_;
#endif // ENABLE_SOMEIP_PACKET_FILTERING

#if defined(ENABLE_QNX_MESSAGE_PASSING)
    std::map<int32_t, std::function<void(void)>> timer_handlers_;
#endif // ENABLE_QNX_MESSAGE_PASSING

#if defined(ENABLE_QNX_MESSAGE_PASSING) || defined(ENABLE_TLS)
private:
    std::mutex mutex_processing_;
#endif // defined(ENABLE_QNX_MESSAGE_PASSING) || defined(ENABLE_TLS)

#if defined(ENABLE_TLS)
public:
    inline std::shared_ptr<lgsomeip::osabstraction::Multiplexer> get_secure_multiplexer() {
        return secure_multiplexer_;
    }

private:
    std::shared_ptr<lgsomeip::osabstraction::Multiplexer> secure_multiplexer_; // Multiplexer for secure connection
#endif                                                                         // ENABLE_TLS
};

} // namespace lgsomeip

#endif // LG_SOMEIP_PACKET_ROUTER_HOST_H
