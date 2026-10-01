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

#include <iomanip>
#include <iostream>
#include <memory>
#include <cctype>
#include <signal.h>
#include <sstream>
#include <chrono>

#include <exception/Exception.h>
#include <message/Message.h>
#include <packetrouter/PacketRouterHost.h>
#include <endpoint/EndpointUtils.h>
#include <utils/time/TimeCheck.h>
#include <utils/log/formatLog.h>
#include <utils/log/logger.h>
#include <utils/byteorder/bytestream.h>

#include <socket/IP4Address.h>
#include <socket/LocalAddress.h>
#include <socket/TCPClientSocket.h>
#include <socket/TCPServerSocket.h>
#include <socket/NetworkDevice.h>

#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
#include <utils/statistics/SomeipPacketStatistics.h>
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS

namespace lgsomeip {

// -----------------------------------------------------------------------------
//  Daemon Process / Signal SIGPIPE Handler
// -----------------------------------------------------------------------------
static void sig_handler(int signo) {
    LGSOMEIP_LOG_FATAL << "PacketRouterHost : sig_handler / signal caught : " << strsignal(signo);
    return;
}

static void signal_init() {
    sigset_t sigset;
    sigset_t sigset_old;
    static sigset_t sigset_test;

    if (signal(SIGPIPE, sig_handler) == SIG_ERR) {
        LGSOMEIP_LOG_FATAL << "PacketRouterHost : SIGPIPE handle can't be registered";
    }

    sigemptyset(&sigset);
    sigaddset(&sigset, SIGPIPE);
    sigprocmask(SIG_BLOCK, &sigset, &sigset_old);
    sigpending(&sigset_test);
    if (sigismember(&sigset_test, SIGPIPE)) {
        LGSOMEIP_LOG_ERROR << "SIGPIPE is pending.";
    }
}

static bool is_valid_application_name(const std::string& name) {
    if (name.empty() || name == "." || name == ".." || name.size() > 128) {
        return false;
    }

    return std::all_of(name.begin(), name.end(), [](char character) {
        const unsigned char value = static_cast<unsigned char>(character);
        return std::isalnum(value) != 0 || character == '-' || character == '_' || character == '.';
    });
}

// -----------------------------------------------------------------------------
//  PacketRouter Host : Public Method
// -----------------------------------------------------------------------------
PacketRouterHost::PacketRouterHost(ServiceManager* host)
    : host_(host), local_receiver_(nullptr), local_message_passing_receiver_(nullptr) {}

PacketRouterHost::~PacketRouterHost() {
    stop();
}

void PacketRouterHost::init() {
    signal_init();
    auto configuration = host_->get_configuration();

    // Initialize Network
    auto& network_device = lgsomeip::osabstraction::NetworkDevice::instance();
    network_device.initialize(configuration->get_address(), configuration->get_ip_type());

    if (network_device.is_initialized()) {
        LGSOMEIP_LOG_INFO << "PacketRouterHost::init / Configured IP Address = " << configuration->get_address() << "("
                          << network_device.get_device_name() << ":" << network_device.get_device_id() << ")";
    } else {
        std::ostringstream sout;
        sout << "Configuration Error! : wrong IP Address [" << "Config IP:" << configuration->get_address() << "]";
        throw LSAR_CONFIGURATION_ERROR(sout.str());
    }

    multiplexer_ = std::make_shared<lgsomeip::osabstraction::Multiplexer>();
#if defined(ENABLE_TLS)
    secure_multiplexer_ = std::make_shared<lgsomeip::osabstraction::Multiplexer>();
#endif // ENABLE_TLS

#if defined(ENABLE_QNX_MESSAGE_PASSING)
    local_message_passing_receiver_ = std::make_shared<EndpointMessagePassingServer>(
        this, EndpointUtils::create_message_passing_channel_name(SOMEIP_DAEMON_ID));
#else
    std::shared_ptr<lgsomeip::osabstraction::TCPServerSocket> local_socket =
        EndpointUtils::create_local_socket<lgsomeip::osabstraction::TCPServerSocket>(SOMEIP_DAEMON_ID, true);
    if (local_socket != nullptr) {
        local_socket->listen();
        local_receiver_ = std::make_shared<EndpointTCPServer<PacketRouterHost>>(this);
        local_receiver_->set_socket(local_socket);
    } else {
        std::ostringstream sout;
        sout << "Configuration Error! / local socket(/tmp/someip/someip-0) creating error!";
        throw LSAR_CONFIGURATION_ERROR(sout.str());
    }
#endif // ENABLE_QNX_MESSAGE_PASSING
#if defined(ENABLE_SOMEIP_PACKET_FILTERING)
    packet_filtering_ = std::make_unique<PacketFiltering>();
    packet_filtering_->set_filter_list(configuration->get_someip_packet_filter_list());
    packet_filtering_->set_period_expire_func(
        [this](std::shared_ptr<Endpoint> endpoint, std::uint8_t* message, std::size_t message_length) {
#if defined(ENABLE_QNX_MESSAGE_PASSING) || defined(ENABLE_TLS)
            std::lock_guard<std::mutex> lock(mutex_processing_);
#endif
            on_external_message(endpoint, message, message_length);
        });
// Unless SOME/IP Delivery Statistics is not used, monitoring packetfiltering, which prints statistics for packet
// filtering, is enabled.
#if !defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
    packet_filtering_->enable_monitor(std::chrono::milliseconds(10000));
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS
#endif // ENABLE_SOMEIP_PACKET_FILTERING

    // open sd port
    auto sd_config = configuration->get_service_discovery_info();

    // For the multicast address
    service_discovery_address_ = make_address(false, sd_config->get_port(), sd_config->get_vlan_priority());
    service_discovery_address_->set_ip_address(sd_config->get_multicast());
    auto new_socket = std::make_shared<lgsomeip::osabstraction::UDPSocket>(service_discovery_address_);
    new_socket->bind();
    new_socket->join_multicast(sd_config->get_multicast().c_str());
    service_discovery_multicast_ = std::make_shared<EndpointUDP<PacketRouterHost>>(this);
    service_discovery_multicast_->set_socket(new_socket);

    struct RoutingConnectionInfo& multicast_info = connected_endpoints_[new_socket->get_socket_fd()];
    multicast_info.endpoint = service_discovery_multicast_;
    multicast_info.is_internal = false;
    multicast_info.app_id = 0;
    multicast_info.endpoint->increase_reference_count();

    LGSOMEIP_LOG_DEBUG << "PacketRouterHost::init / SD fd = " << new_socket->get_socket_fd()
                       << ", IP = " << sd_config->get_multicast() << ", Port = " << sd_config->get_port();

    // Run a thread to create the listening socket for the unicast address receiving SOME/IP-SD packets,
    // since binding socket sometimes takes long due to a series of socket binding failures.
    {
        std::lock_guard<std::mutex> lock(sd_unicast_binding_mutex_);
        sd_unicast_binding_success_ = false;
        sd_unicast_binding_failed_ = false;
        sd_unicast_binding_stop_ = false;
    }

    sd_unicast_binding_thread_ = std::thread([this, configuration, sd_config]() {
        // For assigning thread name
        pthread_setname_np(pthread_self(), "SomeipSockBind");

        {
            std::lock_guard<std::mutex> lock(sd_unicast_binding_mutex_);
            if (sd_unicast_binding_stop_) {
                return;
            }
        }

        auto host_address = make_address(false, sd_config->get_port(), sd_config->get_vlan_priority());
        host_address->set_ip_address(configuration->get_address());
        auto new_host_socket = std::make_shared<lgsomeip::osabstraction::UDPSocket>(host_address);
        if (new_host_socket->bind() == -1) {
            {
                std::lock_guard<std::mutex> lock(sd_unicast_binding_mutex_);
                sd_unicast_binding_failed_ = true;
            }
            sd_unicast_binding_condition_.notify_all();
            return;
        }
        new_host_socket->set_multicast_loop(0); // Loopback off

        {
            std::lock_guard<std::mutex> binding_lock(sd_unicast_binding_mutex_);
            if (sd_unicast_binding_stop_) {
                return;
            }

            service_discovery_unicast_ = std::make_shared<EndpointUDP<PacketRouterHost>>(this);
            service_discovery_unicast_->set_socket(new_host_socket);
        }

        std::lock_guard<std::recursive_mutex> route_lock(route_management_mutex_);
        struct RoutingConnectionInfo& unicast_info = connected_endpoints_[new_host_socket->get_socket_fd()];
        unicast_info.endpoint = service_discovery_unicast_;
        unicast_info.is_internal = false;
        unicast_info.app_id = 0;
        unicast_info.endpoint->increase_reference_count();

        LGSOMEIP_LOG_DEBUG << "PacketRouterHost::init / SD fd = " << new_host_socket->get_socket_fd()
                           << ", IP = " << configuration->get_address() << ", Port = " << sd_config->get_port();

        {
            std::lock_guard<std::mutex> lock(sd_unicast_binding_mutex_);
            sd_unicast_binding_success_ = true;
        }

        sd_unicast_binding_condition_.notify_all();
    });

    if (configuration->is_e2e_enabled()) {
        LGSOMEIP_LOG_INFO << "PacketRouterHost::init / E2E enabled";
        auto e2es = configuration->get_e2e_configs();

        for (auto& e2e : e2es) {
            auto e2e_config = e2e.second;
            auto e2e_identifier = e2e.first;
            if (e2e_config->get_profile() == "CRC32") {
                vsomeip::e2e::profile_custom::profile_config profile_config =
                    vsomeip::e2e::profile_custom::profile_config(e2e_config->get_crc_offset());

                if ((e2e_config->get_variant() == "protector") || (e2e_config->get_variant() == "both")) {
                    e2e_custom_protectors_[e2e_identifier] =
                        std::make_shared<vsomeip::e2e::profile_custom::protector>(profile_config);
                }
                if ((e2e_config->get_variant() == "checker") || (e2e_config->get_variant() == "both")) {
                    e2e_custom_checkers_[e2e_identifier] =
                        std::make_shared<vsomeip::e2e::profile_custom::profile_custom_checker>(profile_config);
                }
            } else if (e2e_config->get_profile() == "CRC8") {
                vsomeip::e2e::profile01::profile_config profile_config = vsomeip::e2e::profile01::profile_config(
                    e2e_config->get_crc_offset(), e2e_config->get_data_id(),
                    (vsomeip::e2e::profile01::p01_data_id_mode)e2e_config->get_data_id_mode(),
                    e2e_config->get_data_length(), e2e_config->get_counter_offset(),
                    e2e_config->get_data_id_nibble_offset());

                if ((e2e_config->get_variant() == "protector") || (e2e_config->get_variant() == "both")) {
                    e2e_custom_protectors_[e2e_identifier] =
                        std::make_shared<vsomeip::e2e::profile01::protector>(profile_config);
                }
                if ((e2e_config->get_variant() == "checker") || (e2e_config->get_variant() == "both")) {
                    e2e_custom_checkers_[e2e_identifier] =
                        std::make_shared<vsomeip::e2e::profile01::profile_01_checker>(profile_config);
                }
            }

            LGSOMEIP_LOG_INFO << "E2E - " << format_named_id("ServiceID", e2e.second->get_service_id(), 4) << " "
                              << format_named_id("EventID", e2e.second->get_event_id(), 4);
        }
    }
}

void PacketRouterHost::start() {
    multiplexer_->start();
#if defined(ENABLE_TLS)
    secure_multiplexer_->start();
#endif // ENABLE_TLS

    if (local_receiver_ != nullptr) {
        local_receiver_->start_listen(multiplexer_);
        local_receiver_->start_listen();
    }

    if (local_message_passing_receiver_ != nullptr) {
        local_message_passing_receiver_->start_listen();
    }

    service_discovery_multicast_->start_listen(multiplexer_);

    // Run a thread to create the listening socket for the unicast address receiving SOME/IP-SD packets,
    // since binding socket sometimes takes long due to a series of socket binding failures.
    sd_unicast_start_listen_thread_ = std::thread([this]() {
        // For assigning thread name
        pthread_setname_np(pthread_self(), "SomeipSockBind");

        // Wait until socket binding finishes or the router is stopped.
        {
            std::unique_lock<std::mutex> lock(sd_unicast_binding_mutex_);
            sd_unicast_binding_condition_.wait(lock, [this]() {
                return sd_unicast_binding_success_ || sd_unicast_binding_failed_ || sd_unicast_binding_stop_;
            });
            if (!sd_unicast_binding_success_ || sd_unicast_binding_stop_) {
                return;
            }
        }

        service_discovery_unicast_->start_listen(multiplexer_);

        LGSOMEIP_LOG_INFO << "PacketRouterHost::start / Creating SOME/IP-SD unicast socket succeeded.";
    });
}

void PacketRouterHost::stop() {
    {
        std::lock_guard<std::mutex> lock(sd_unicast_binding_mutex_);
        sd_unicast_binding_stop_ = true;
    }
    sd_unicast_binding_condition_.notify_all();

    if (sd_unicast_binding_thread_.joinable()) {
        sd_unicast_binding_thread_.join();
    }
    if (sd_unicast_start_listen_thread_.joinable()) {
        sd_unicast_start_listen_thread_.join();
    }

    // disconnect Receiver Socket to Multiplexer
    if (local_receiver_ != nullptr) {
        local_receiver_->stop_listen();
    }

    if (local_message_passing_receiver_ != nullptr) {
        local_message_passing_receiver_->stop_listen();
    }

    if (multiplexer_ != nullptr) {
        multiplexer_->stop();
        multiplexer_->join();
    }

#if defined(ENABLE_TLS)
    if (secure_multiplexer_ != nullptr) {
        secure_multiplexer_->stop();
        secure_multiplexer_->join();
    }
#endif // ENABLE_TLS
}

void PacketRouterHost::on_connect(std::shared_ptr<Endpoint> server_endpoint,
                                  std::shared_ptr<Endpoint> client_endpoint) {
#if defined(ENABLE_QNX_MESSAGE_PASSING)
    std::lock_guard<std::mutex> lock(mutex_processing_);
#endif // ENABLE_QNX_MESSAGE_PASSING

    std::uint32_t serverfd = server_endpoint->get_socket()->get_socket_fd();
    std::uint32_t clientfd = client_endpoint->get_socket()->get_socket_fd();

    std::lock_guard<std::recursive_mutex> guard(route_management_mutex_);

    if (local_receiver_ == server_endpoint) {
        struct RoutingConnectionInfo info;
        info.endpoint = client_endpoint;
        info.is_internal = true;
        info.app_id = 0;
        info.endpoint->increase_reference_count();
        connected_endpoints_[clientfd] = info;
#if defined(ENABLE_TLS)
        if (client_endpoint->get_socket()->is_secure_connection()) {
            client_endpoint->start_listen(get_secure_multiplexer());
        } else {
            client_endpoint->start_listen(get_multiplexer());
        }
#else  // ENABLE_TLS
        client_endpoint->start_listen(get_multiplexer());
#endif // ENABLE_TLS
    } else {
        // TODO : implement connection management for process another connection
        LGSOMEIP_LOG_DEBUG << "PacketRouterHost::on_connect / TCP Server receives connection from "
                           << client_endpoint->get_socket()->get_dst_address()->to_string() << " to "
                           << server_endpoint->get_socket()->get_src_address()->to_string();

        struct RoutingConnectionInfo info;
        info.endpoint = client_endpoint;
        info.is_internal = false;
        info.app_id = connected_endpoints_[serverfd].app_id;
        info.serverfd = serverfd;
        info.endpoint->increase_reference_count();
        connected_endpoints_[clientfd] = info;
#if defined(ENABLE_TLS)
        if (client_endpoint->get_socket()->is_secure_connection()) {
            client_endpoint->start_listen(get_secure_multiplexer());
        } else {
            client_endpoint->start_listen(get_multiplexer());
        }
#else  // ENABLE_TLS
        client_endpoint->start_listen(get_multiplexer());
#endif // ENABLE_TLS
    }
}

void PacketRouterHost::reboot_route(std::shared_ptr<lgsomeip::osabstraction::Address> address) {
    if (address != nullptr) {
        auto sender_address = address;
        std::string sender_ip_address = sender_address->get_ip_address();

        LGSOMEIP_LOG_DEBUG << "PacketRouterHost::reboot_route / Remove subscribe info "
                           << "[Addr:" << sender_address->to_string() << "]";

        // Remove subscribe info related to the addr
        remove_subscribe_route(sender_address);

        auto it = request_map_.begin();
        while (it != request_map_.end()) {
            if (it->second.targeaddr != nullptr) {
                auto mapfd = it->second.targeaddr->get_ip_address();
                if (mapfd == sender_ip_address)
                    it = request_map_.erase(it);
                else
                    ++it;
            } else {
                ++it;
            }
        }
    }
}

void PacketRouterHost::on_disconnect(std::shared_ptr<Endpoint> endpoint) {
#if defined(ENABLE_QNX_MESSAGE_PASSING)
    std::lock_guard<std::mutex> lock(mutex_processing_);
#endif // ENABLE_QNX_MESSAGE_PASSING
    std::int32_t fd = endpoint->get_socket()->get_socket_fd();
    LGSOMEIP_LOG_INFO << "PacketRouterHost::on_disconnect / FD = " << fd;

    std::lock_guard<std::recursive_mutex> guard(route_management_mutex_);

    auto conn_info_it = connected_endpoints_.find(fd);
    if (conn_info_it == connected_endpoints_.end()) {
        LGSOMEIP_LOG_WARN << "PacketRouterHost::on_disconnect / There is no endpoint for FD = " << fd;

        return; // Error
    }

    auto& connection_info = conn_info_it->second;

    // Disconnection from internal app
    if (connection_info.is_internal == true) {
        std::uint16_t app_id = connection_info.app_id;
        LGSOMEIP_LOG_INFO << "PacketRouterHost::on_disconnect / Listener Stop " << format_named_id("AppID", app_id, 4);

        // If appId = 0, there is a socket error before receiving ApplicationControlMessage.
        if (app_id == 0) {
            LGSOMEIP_LOG_INFO << "PacketRouterHost::on_disconnect / there is a socket error before receiving "
                              << "ApplicationControlMessage, FD = " << fd;

            remove_app_before_connection(fd, connection_info);
            return;
        }

        // Remove service info related to the appId
        remove_subscribe_route(app_id);

        // Remove route info for all provided/consumed services related with appId.
        remove_route(app_id);

        // Remove request map for routing request/response.
        auto req_map_it = request_map_.begin();
        while (req_map_it != request_map_.end()) {
            if (req_map_it->second.endpoint == local_applications_[app_id].sender) {
                req_map_it = request_map_.erase(req_map_it);
            } else {
                ++req_map_it;
            }
        }

        // Stop Application Socket
        local_applications_[app_id].receiver->stop_listen();
        local_applications_.erase(app_id);

        connection_info.endpoint->decrease_reference_count();

        LGSOMEIP_LOG_INFO << "PacketRouterHost::on_disconnect / referenceCount of this endpoint = "
                          << static_cast<std::uint16_t>(connection_info.endpoint->get_reference_count()) << ", "
                          << format_named_id("AppID", app_id, 4);

        if (connection_info.endpoint->get_reference_count() == 0) {
            connected_endpoints_.erase(fd);
        }
        local_receiver_->remove_client_endpoint(fd);
        host_->on_disconnected_application(app_id);
    }
    // Disconnection on external service endpoint
    else {
        std::shared_ptr<lgsomeip::osabstraction::Address> addr;

        // Our TCP server socket gets disconnected.
        // This case will barely happen.
        if (connection_info.serverfd == 0) {
            addr = endpoint->get_socket()->get_src_address();
            LGSOMEIP_LOG_INFO << "PacketRouterHost::on_disconnect / Disconnected on TCP server Endpoint "
                              << "[Addr:" << addr->to_string() << "]";

            auto client_list =
                std::dynamic_pointer_cast<EndpointTCPServer<PacketRouterHost>>(endpoint)->get_client_list();
            for (auto& client : client_list) {
                auto client_it = connected_endpoints_.find(client);
                if (client_it != connected_endpoints_.end()) {
                    auto& client_conn_info = client_it->second;
                    client_conn_info.endpoint->stop_listen();
                    client_conn_info.endpoint->decrease_reference_count();

                    LGSOMEIP_LOG_INFO << "PacketRouterHost::on_disconnect / referenceCount of TCP client endpoint = "
                                      << static_cast<std::uint16_t>(client_conn_info.endpoint->get_reference_count())
                                      << ", client FD = " << client << ", server FD = " << fd;

                    if (client_conn_info.endpoint->get_reference_count() == 0) {
                        connected_endpoints_.erase(client);
                    }
                }
            }
            endpoint->stop_listen();

            LGSOMEIP_LOG_INFO << "PacketRouterHost::on_disconnect / Remove this TCP server endpoint," << "FD = " << fd;

            connected_endpoints_.erase(fd);
        }
        // Our TCP client socket is disconnected.
        else {
            addr = endpoint->get_socket()->get_dst_address();
            LGSOMEIP_LOG_INFO << "PacketRouterHost::on_disconnect / Disconnected on TCP client Endpoint "
                              << "[Addr:" << addr->to_string() << "]";

            // Remove subscribe info related to the addr
            remove_subscribe_route(addr);

            // Remove request map for routing request/response.
            auto req_map_it = request_map_.begin();
            while (req_map_it != request_map_.end()) {
                auto map_fd = req_map_it->second.endpoint->get_socket()->get_socket_fd();
                if (map_fd == fd) {
                    req_map_it = request_map_.erase(req_map_it);
                } else {
                    ++req_map_it;
                }
            }

            // Disconnected from provider (we are consumer)
            if (connection_info.serverfd == -1) {
                remove_route(addr);
            }
            // Disconnected from consumer (we are provider)
            else {
                connection_info.endpoint->decrease_reference_count();
                LGSOMEIP_LOG_INFO << "PacketRouterHost::on_disconnect / referenceCount of this endpoint = "
                                  << static_cast<std::uint16_t>(connection_info.endpoint->get_reference_count())
                                  << ", addr =" << addr->to_string();

                if (connection_info.endpoint->get_reference_count() == 0) {
                    connected_endpoints_.erase(fd);
                }

                auto ep = std::dynamic_pointer_cast<EndpointTCPClient<PacketRouterHost>>(endpoint);
                if (ep->get_server_endpoint() != nullptr) {
                    LGSOMEIP_LOG_INFO << "PacketRouterHost::on_disconnect / Remove this TCP client endpoint [FD:" << fd
                                      << "] from TCP server endpoint";
                    ep->get_server_endpoint()->remove_client_endpoint(fd);
                }
            }
        }

        host_->on_disconnected_service(addr);
    }
}

/*
std::shared_ptr<lgsomeip::osabstraction::Address> PacketRouterHost::get_address(std::uint16_t app_id)
{
    std::shared_ptr<lgsomeip::osabstraction::Address> addr = nullptr;

    if (local_applications_.find(app_id) != local_applications_.end()) {
        addr = local_applications_[app_id].sender->get_socket()->get_dst_address();
    }

    return addr;
}
*/

bool PacketRouterHost::is_reboot(const std::shared_ptr<Endpoint> endpoint, bool reboot_flag, std::uint32_t request_id) {
    LGSOMEIP_LOG_DEBUG << "PacketRouterHost::is_reboot Check reboot";

    auto src_addr = endpoint->get_sender_address();
    std::string sender_ip_address = src_addr->get_ip_address();

    auto dst_addr = endpoint->get_socket()->get_src_address();
    std::string receiver_ip_address = dst_addr->get_ip_address();

    uint16_t temp_session = static_cast<uint16_t>(request_id & 0x0000FFFF);
    bool temp_reboot_flag = reboot_flag;

    uint16_t prev_session;
    bool prev_reboot_flag;

    bool reboot_detect(false);

    auto temp_key = std::make_pair(sender_ip_address, receiver_ip_address);

    auto it = reboot_info_.find(temp_key);
    if (it != reboot_info_.end()) {
        prev_session = (it->second).first;
        prev_reboot_flag = (it->second).second;

        if ((prev_reboot_flag == 0 && temp_reboot_flag == 1) ||
            (prev_reboot_flag == 1 && temp_reboot_flag == 1 && prev_session >= temp_session)) {
            reboot_detect = true;
            LGSOMEIP_LOG_INFO << "PacketRouterHost::is_reboot / Reboot was detected"
                              << ", peer_address : " << sender_ip_address << ", prevRbtFg: " << prev_reboot_flag
                              << ", crntRbtFg: " << temp_reboot_flag << ", "
                              << format_named_id("PreviousSessionID", prev_session, 4) << ", "
                              << format_named_id("CurrentSessionID", temp_session, 4);
        }
    }

    reboot_info_[temp_key] = std::make_pair(temp_session, temp_reboot_flag);

    return reboot_detect;
}

void PacketRouterHost::process_trigger_writing_service_state(void) {
    LGSOMEIP_LOG_INFO << "PacketRouterHost::process_trigger_writing_service_state / Start";

    // Write the current states of SOME/IP services to files
    // Create the list having the current connected applications
    std::map<std::uint16_t, std::string> list_apps;

    for (auto& element : local_applications_) {
        LGSOMEIP_LOG_INFO << "PacketRouterHost::process_trigger_writing_service_state / "
                          << format_named_id("AppID", element.first, 4) << ", app_name: " << element.second.name;

        // If an application uses only local SOME/IP services, it should not inlcuded in the list
        if ((element.second.name.compare("no-name") == 0) || (element.second.name.compare("") == 0)) {
            continue;
        }

        list_apps[element.first] = element.second.name;
    }

    // Try to write the current states of SOME/IP services in the ServiceManager class
    if (!host_->write_current_state_someip_services(list_apps)) {
        LGSOMEIP_LOG_INFO << "PacketRouterHost::process_trigger_writing_service_state / Fail to write files!!!";
    }

    LGSOMEIP_LOG_INFO << "PacketRouterHost::process_trigger_writing_service_state / End";
}

void PacketRouterHost::on_message(std::shared_ptr<Endpoint> endpoint, std::uint8_t* message,
                                  std::size_t message_length) {
#if defined(ENABLE_QNX_MESSAGE_PASSING) || defined(ENABLE_TLS)
    std::lock_guard<std::mutex> lock(mutex_processing_);
#endif // defined(ENABLE_QNX_MESSAGE_PASSING) || defined(ENABLE_TLS)

    print_byte_message("PacketRouterHost::on_message", message, message_length);

    bool is_sd_message = MessageBuilder::is_sd_message(message, message_length);

    std::unique_lock<std::recursive_mutex> lock_manage_route(route_management_mutex_);
    bool from_internal = connected_endpoints_[endpoint->get_socket()->get_socket_fd()].is_internal;
    lock_manage_route.unlock();

    if (is_sd_message) {
        // Check if this message is for trigger to create the files including the current states of SOME/IP services
        if (message_length >= 26 && message[24] == 0xF1 && message[25] == 0xF2) {
#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
            SomeipPacketStatistics::get_instance().increase_sd_type_packet(
                SomeipPacketStatistics::kIncoming, 0, SomeipPacketStatistics::kSomeipSdTriggerDumpSomeipServices);
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS

            if (message[12] != 0 || message[13] != 0 || message[14] != 2 || message[15] != 0) {
                LGSOMEIP_LOG_WARN << "PacketRouterHost::on_message / Invalid trigger message";

                return;
            }

            // Create the files including the current states of SOME/IP services
            process_trigger_writing_service_state();

            return;
        }

        auto sd_message = MessageBuilder::create<SOMEIPSD>();
        // Deserialization error in MessageSD(SDEntry, SDOption) returns true to send wrong SD Message(Nack).
        if (false == MessageBuilder::build_message(*sd_message, message, message_length)) {
            LGSOMEIP_LOG_WARN << "PacketRouterHost::on_message / Fail to build_message of the SOMEIPSD!!!";
            return;
        }

        if (from_internal) {
            if (sd_message->entries().empty()) {
                LGSOMEIP_LOG_WARN << "PacketRouterHost::on_message / SD message has no entries";
                return;
            }
            if (sd_message->entry(0).get_type() == SOMEIP_SD_ENTRY::INTERNAL::TYPEID) {
                // Process Applcation Control Message
                on_application_control_message(endpoint, sd_message);
            } else {
                // Process Service Control Message : send to service manager
                host_->on_internal_message(sd_message);
            }
        } else {
            bool reboot = is_reboot(endpoint, sd_message->get_reboot_flag(), sd_message->get_request_id());

            auto sender = endpoint->get_sender_address();
            auto config = host_->get_configuration();

            if (sender->get_port_address() != config->get_service_discovery_info()->get_port()) {
                LGSOMEIP_LOG_INFO << "PacketRouterHost::on_message / Unknown SD port number. Message Discarded";

#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
                SomeipPacketStatistics::get_instance().increase_sd_type_packet(
                    SomeipPacketStatistics::kIncoming, 0, SomeipPacketStatistics::kSomeipSdUnknownSdPort);
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS

                return;
            }

            if (sender->get_ip_address() == config->get_address()) {
                LGSOMEIP_LOG_DEBUG << "PacketRouterHost::on_message / Message Discarded! sender: "
                                   << sender->get_ip_address().c_str()
                                   << ", config_addr: " << config->get_address().c_str() << "| receiver: "
                                   << endpoint->get_socket()->get_src_address()->get_ip_address().c_str();

#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
                SomeipPacketStatistics::get_instance().increase_sd_type_packet(
                    SomeipPacketStatistics::kIncoming, 0, SomeipPacketStatistics::kSomeipSdInvalidMessage);
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS

                return;
            }

            // Process Service Control Message : send to service manager
            host_->on_external_message(endpoint, sd_message, reboot);
        }

        return;
    }

#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
    std::uint32_t incoming_message_id;
    get_byte_stream(&incoming_message_id, message);
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS

    // SOME/IP Message
    if (from_internal) {
#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
        SomeipPacketStatistics::get_instance().increase_received_packet(SomeipPacketStatistics::kOutgoing,
                                                                        incoming_message_id);
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS

        on_internal_message(endpoint, message, message_length);
    } else {
#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
        SomeipPacketStatistics::get_instance().increase_received_packet(SomeipPacketStatistics::kIncoming,
                                                                        incoming_message_id);
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS
#if defined(ENABLE_SOMEIP_PACKET_FILTERING)
        std::uint32_t message_id;

        if (check_byte_order() == true) {
            // Little endian
            message_id = (std::uint32_t)message[0] << 24 | (std::uint32_t)message[1] << 16 |
                         (std::uint32_t)message[2] << 8 | (std::uint32_t)message[3];
        } else {
            // Big endian
            message_id = (std::uint32_t)message[3] << 24 | (std::uint32_t)message[2] << 16 |
                         (std::uint32_t)message[1] << 8 | (std::uint32_t)message[0];
        }

        if (packet_filtering_->filter_message(message_id, endpoint, message, message_length) ==
            PacketFiltering::kBlocked) {
            LGSOMEIP_LOG_DEBUG << "PacketRouterHost::on_message / This packet is blocked to prevent overload "
                               << format_named_id("MessageID", message_id, 8);

            return;
        } else {
            LGSOMEIP_LOG_DEBUG << "PacketRouterHost::on_message / This packet is forwarded to Apps "
                               << format_named_id("MessageID", message_id, 8);
        }
#endif // ENABLE_SOMEIP_PACKET_FILTERING
        on_external_message(endpoint, message, message_length);
    }
}

// -----------------------------------------------------------------------------
//  PacketRouter Host : Mapping (Service ID, Socket Addr) to Instance ID
// -----------------------------------------------------------------------------
/*
 *  Service-Instances of the same Service are identified through
 *                             different Instance IDs
 *  Multiple Service-Instances of the same service on one single
 *                             ECU shall listen on different ports per Service-Instance
 *
 * A Service Instance can be identified through the combination of the Service ID combined
 * with the socket (i.e. IP-address, transport protocol (UDP/TCP), and port number).
 */

static std::uint32_t PORT_TCPTAG = 0x00010000;
static std::uint32_t PORT_UDPTAG = 0x00020000;
static std::uint32_t PORT_LOCTAG = 0x00040000;

void PacketRouterHost::set_instance_id(std::uint16_t service_id, std::uint16_t instance_id, std::uint16_t app_id) {
    std::uint32_t addrid = PORT_LOCTAG | (static_cast<std::uint32_t>(app_id) & 0xffff);
    instance_id_map_[service_id][addrid] = instance_id;
}

void PacketRouterHost::set_instance_id(std::uint16_t service_id, std::uint16_t instance_id,
                                       std::shared_ptr<lgsomeip::osabstraction::Address> addr) {
    std::uint32_t addrid = 0;

    if (addr->get_type() != AF_UNIX) {
        if (addr->get_reliable()) {
            addrid = PORT_TCPTAG | (static_cast<std::uint32_t>(addr->get_port_address()) & 0xffff);
        } else {
            addrid = PORT_UDPTAG | (static_cast<std::uint32_t>(addr->get_port_address()) & 0xffff);
        }
        instance_id_map_[service_id][addrid] = instance_id;
    }
}

std::uint16_t PacketRouterHost::find_instance_id(std::uint16_t service_id, std::shared_ptr<Endpoint> endpoint) {
    std::uint16_t iid = 0;
    std::uint32_t addrid = 0;

    if ((endpoint.get() != nullptr) && (endpoint->get_socket().get() == nullptr)) {
        if (endpoint->get_instance_id() != 0) {
            return endpoint->get_instance_id();
        }

        std::uint16_t app_id = endpoint->get_app_id();
        addrid = PORT_LOCTAG | (static_cast<std::uint32_t>(app_id) & 0xffff);
    } else {
        std::shared_ptr<lgsomeip::osabstraction::Address> addr = endpoint->get_socket()->get_src_address();
        if (addr == nullptr) {
            addr = endpoint->get_socket()->get_dst_address();
        }

        if (addr->get_type() == AF_UNIX) {
            LGSOMEIP_LOG_DEBUG << "PacketRouterHost::find_instance_id / "
                               << format_named_id("InstanceID", endpoint->get_instance_id(), 4);

            // If this endpoint has an instanceID, this function just returns the instanceID.
            if (endpoint->get_instance_id() != 0) {
                return endpoint->get_instance_id();
            }
            std::uint16_t app_id = connected_endpoints_[endpoint->get_socket()->get_socket_fd()].app_id;
            addrid = PORT_LOCTAG | (static_cast<std::uint32_t>(app_id) & 0xffff);
        } else {
            if (addr->get_reliable() == true) {
                auto ep = std::dynamic_pointer_cast<EndpointTCPClient<PacketRouterHost>>(endpoint);

                if (ep->get_server_endpoint() != nullptr) {
                    addr = ep->get_server_endpoint()->get_socket()->get_src_address();
                }
                addrid = PORT_TCPTAG | (static_cast<std::uint32_t>(addr->get_port_address()) & 0xffff);
            } else {
                addrid = PORT_UDPTAG | (static_cast<std::uint32_t>(addr->get_port_address()) & 0xffff);
            }
        }
    }

    auto svcmap = instance_id_map_.find(service_id);
    if (svcmap == instance_id_map_.end())
        return 0;

    auto iter = svcmap->second.find(addrid);
    if (iter != svcmap->second.end()) {
        iid = iter->second;
    }

    // LGSOMEIP_LOG_DEBUG << "PacketRouterHost::find_instance_id / List";
    // for (auto& svc : svcmap->second) {
    //     LGSOMEIP_LOG_DEBUG << "PacketRouterHost::find_instance_id"
    //                        << " / addrid:" << MSGID_FORMAT6(svc.first)
    //                        << " / iid: " << MSGID_FORMAT4(svc.second);
    // }

    LGSOMEIP_LOG_DEBUG << "PacketRouterHost::find_instance_id / " << format_named_id("ServiceID", service_id, 4)
                       << " => " << format_named_id("InstanceID", iid, 4);

    return iid;
}

struct RoutingServiceInfo* PacketRouterHost::get_service_instance(std::uint16_t service_id,
                                                                  std::shared_ptr<Endpoint> endpoint) {
    if ((endpoint.get() != nullptr) && (endpoint->get_socket().get() != nullptr)) {
        std::shared_ptr<lgsomeip::osabstraction::Address> addr = endpoint->get_socket()->get_src_address();
        if (addr == nullptr) {
            addr = endpoint->get_socket()->get_dst_address();
        }

        LGSOMEIP_LOG_DEBUG << "PacketRouterHost::get_service_instance / addr = " << addr->to_string()
                           << "[fd:" << endpoint->get_socket()->get_socket_fd() << "]";
    }

    std::lock_guard<std::recursive_mutex> guard(route_management_mutex_);

    struct RoutingServiceInfo* info = nullptr;
    std::uint16_t iid = find_instance_id(service_id, endpoint);
    if (iid == 0)
        return nullptr;

    auto serviceinfo = registered_service_info_.find(service_id);
    if (serviceinfo != registered_service_info_.end()) {
        auto instanceinfo = serviceinfo->second.find(iid);
        if (instanceinfo != serviceinfo->second.end()) {
            info = &(instanceinfo->second);
            endpoint->set_instance_id(iid);
        }
    }

    if (info != nullptr) {
        LGSOMEIP_LOG_DEBUG << "PacketRouterHost::get_service_instance / "
                           << format_named_id("ServiceID", service_id, 4);

        if (info->app_id == 0) {
            if (info->udpendpoint != nullptr && info->udpserveraddress != nullptr) {
                LGSOMEIP_LOG_DEBUG << " => UDP / Dst Address : " << info->udpserveraddress->to_string();
            } else if (info->tcpendpoint != nullptr) {
                LGSOMEIP_LOG_DEBUG << " => TCP / Dst Address : "
                                   << info->tcpendpoint->get_socket()->get_dst_address()->to_string();
            }
        } else {
            LGSOMEIP_LOG_DEBUG << " => " << format_named_id("AppID", info->app_id, 4);
        }
    } else {
        LGSOMEIP_LOG_DEBUG << "PacketRouterHost::get_service_instance / No Service Info "
                           << format_named_id("ServiceID", service_id, 4);
    }

    return info;
}

// -----------------------------------------------------------------------------
//  PacketRouter Host : Internal / Callback for Data Message
// -----------------------------------------------------------------------------
// Callback for Data Message
void PacketRouterHost::on_internal_message(std::shared_ptr<Endpoint> endpoint, std::uint8_t* message,
                                           std::size_t message_length) {
    MessageHeader header;
    bool success = header.deserialize(message, message_length);
    if (!success) {
        return;
    }

    std::uint8_t type = header.get_message_type();
    std::uint32_t messageid = header.get_message_id();
    std::uint32_t requestid = header.get_request_id();
    auto configuration = host_->get_configuration();
    vsomeip::e2e_buffer output_buffer, input_buffer;

    if (configuration->is_e2e_enabled()) {
        std::uint16_t serviceid = (std::uint16_t)(messageid >> 16);
        std::uint16_t methodid = (std::uint16_t)(messageid & 0xffff);
        std::pair<std::uint16_t, std::uint16_t> identifier = {serviceid, methodid};

        auto it = e2e_custom_protectors_.find(identifier);
        if (it != e2e_custom_protectors_.end()) {
            output_buffer.assign(message, message + SOMEIP_HEADER::SIZE);                 // SOMEIP HEADER
            input_buffer.assign(message + SOMEIP_HEADER::SIZE, message + message_length); // PAYLOAD
            (it->second)->protect(input_buffer);
            output_buffer.resize(input_buffer.size() + SOMEIP_HEADER::SIZE);
            std::copy(input_buffer.begin(), input_buffer.end(), output_buffer.begin() + SOMEIP_HEADER::SIZE);
            message = output_buffer.data();
        }
    }

    switch (type) {
    case SOMEIP_MESSAGE_TYPE::REQUEST_NO_RETURN:
    case SOMEIP_MESSAGE_TYPE::TP_REQUEST_NO_RETURN:
        on_internal_request(endpoint, message, message_length, messageid, requestid, true);
        break;
    case SOMEIP_MESSAGE_TYPE::REQUEST:
    case SOMEIP_MESSAGE_TYPE::TP_REQUEST:
        on_internal_request(endpoint, message, message_length, messageid, requestid);
        break;
    case SOMEIP_MESSAGE_TYPE::RESPONSE:
    case SOMEIP_MESSAGE_TYPE::TP_RESPONSE:
    case SOMEIP_MESSAGE_TYPE::ERROR:
    case SOMEIP_MESSAGE_TYPE::TP_ERROR:
        on_internal_response(endpoint, message, message_length, messageid, requestid);
        break;
    case SOMEIP_MESSAGE_TYPE::NOTIFICATION:
    case SOMEIP_MESSAGE_TYPE::TP_NOTIFICATION:
        on_internal_notification(endpoint, message, message_length, messageid, requestid);
        break;
    default:
        break;
    }
}

void PacketRouterHost::on_internal_request(std::shared_ptr<Endpoint> endpoint, std::uint8_t* message,
                                           std::size_t message_length, std::uint32_t message_id,
                                           std::uint32_t request_id, bool no_response) {
    std::uint16_t app_id = (std::uint16_t)(request_id >> 16);
    std::uint16_t sid = (std::uint16_t)(message_id >> 16);

    auto serviceinfo = get_service_instance(sid, endpoint);
    if (serviceinfo == nullptr)
        return;

    LGSOMEIP_LOG_DEBUG << "PacketRouterHost::on_internal_request "
                       << format_service_instance_id(sid, endpoint->get_instance_id()) << " "
                       << format_named_id("RequestID", request_id, 8);

    if (no_response == false) {
        std::uint16_t clientid = static_cast<std::uint16_t>(request_id >> 16);
        auto& info = request_map_[clientid];
        info.ttl = 5;
        info.endpoint = local_applications_[app_id].sender;
    }

    if (serviceinfo->svcendpoint != nullptr) {
        // Send to Internal Application
        // Add instanceID in the end of the message if this message is for internal
        std::uint16_t instance_id = endpoint->get_instance_id();
        set_byte_stream(message + message_length, &instance_id, 2);
        message_length += 2;

        serviceinfo->svcendpoint->send_message(message, message_length);

#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
        SomeipPacketStatistics::get_instance().increase_delivered_packet(SomeipPacketStatistics::kOutgoing, message_id);
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS
    } else {
        // Send to External Application
        if (serviceinfo->tcpendpoint) {
            serviceinfo->tcpendpoint->send_message(message, message_length);
        }
        if (serviceinfo->udpendpoint) {
            serviceinfo->udpendpoint->send_message(message, message_length, serviceinfo->udpserveraddress);
        }

#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
        if (serviceinfo->tcpendpoint != nullptr || serviceinfo->udpendpoint != nullptr) {
            SomeipPacketStatistics::get_instance().increase_delivered_packet(SomeipPacketStatistics::kOutgoing,
                                                                             message_id);
        } else {
            SomeipPacketStatistics::get_instance().increase_dropped_packet(
                SomeipPacketStatistics::kOutgoing, message_id, SomeipPacketStatistics::kPacketRouterHostNoConnection);
        }
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS
    }
}

void PacketRouterHost::on_internal_response(std::shared_ptr<Endpoint> endpoint, std::uint8_t* message,
                                            std::size_t message_length, std::uint32_t message_id,
                                            std::uint32_t request_id) {
    const std::uint16_t sid = static_cast<std::uint16_t>(message_id >> 16);
    LGSOMEIP_LOG_DEBUG << "PacketRouterHost::on_internal_response "
                       << format_service_instance_id(sid, endpoint->get_instance_id()) << " "
                       << format_named_id("RequestID", request_id, 8);

    std::uint16_t clientid = static_cast<std::uint16_t>(request_id >> 16);
    if (request_map_.find(clientid) == std::end(request_map_))
        return;

    auto config = host_->get_configuration();
    auto& info = request_map_[clientid];
    auto& request_endpoint = info.endpoint;
    if (request_endpoint != nullptr) {
        // Add instanceID in the end of the message if this message is for internal
        if (nullptr == info.targeaddr || config->get_address() == info.targeaddr->get_ip_address()) {
            std::uint16_t instance_id = request_endpoint->get_instance_id();
            set_byte_stream(message + message_length, &instance_id, 2);
            message_length += 2;

            LGSOMEIP_LOG_DEBUG << "PacketRouterHost::on_internal_response  / Send to Internal Application";
        }

        if ((request_endpoint.get() != nullptr) && (request_endpoint->get_socket().get() == nullptr)) {
            request_endpoint->send_message(message, message_length);
        } else {
            if (request_endpoint->get_socket()->get_reliable() == false) {
                request_endpoint->send_message(message, message_length, info.targeaddr);
            } else {
                request_endpoint->send_message(message, message_length);
            }
        }

#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
        SomeipPacketStatistics::get_instance().increase_delivered_packet(SomeipPacketStatistics::kOutgoing, message_id);
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS
    }
}

void PacketRouterHost::on_internal_notification(std::shared_ptr<Endpoint> endpoint, std::uint8_t* message,
                                                std::size_t message_length, std::uint32_t message_id,
                                                std::uint32_t request_id) {
    std::uint16_t serviceid = (std::uint16_t)(message_id >> 16);
    std::uint16_t eventid = (std::uint16_t)(message_id & 0xffff);
    std::uint16_t target_client = static_cast<std::uint16_t>(request_id >> 16);
    const std::uint32_t original_request_id = request_id;
    std::vector<std::uint16_t> multicast_eventgroups;

    LGSOMEIP_LOG_DEBUG << "PacketRouterHost::on_internal_notification "
                       << format_service_instance_event_id(serviceid, endpoint->get_instance_id(), eventid) << " "
                       << format_named_id("RequestID", request_id, 8);

    auto serviceinfo = get_service_instance(serviceid, endpoint);
    if (serviceinfo == nullptr) {
        LGSOMEIP_LOG_INFO << "PacketRouterHost::on_internal_notification "
                          << format_service_instance_event_id(serviceid, endpoint->get_instance_id(), eventid) << " "
                          << format_named_id("RequestID", request_id, 8) << " No Service Information";

#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
        SomeipPacketStatistics::get_instance().increase_dropped_packet(
            SomeipPacketStatistics::kOutgoing, message_id, SomeipPacketStatistics::kPacketRouterHostNoServiceInfo);
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS

        return;
    }

    std::vector<struct RoutingSubscribeInfo>* subscribe_list_ptr = nullptr;
    auto subscribe_list_it = serviceinfo->eventinfos.find(eventid);
    if (subscribe_list_it == serviceinfo->eventinfos.end()) {
        if (serviceinfo->eventinfos.find(SOMEIP_DEFAULT_ANY_EVENT) == serviceinfo->eventinfos.end()) {
            LGSOMEIP_LOG_DEBUG << "PacketRouterHost::on_internal_notification "
                               << format_service_instance_event_id(serviceid, endpoint->get_instance_id(), eventid)
                               << " " << format_named_id("RequestID", request_id, 8) << " No subscription information";

#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
            SomeipPacketStatistics::get_instance().increase_dropped_packet(
                SomeipPacketStatistics::kOutgoing, message_id,
                SomeipPacketStatistics::kPacketRouterHostNoSubscriptionInfo);
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS

            return;
        } else {
            LGSOMEIP_LOG_DEBUG
                << "PacketRouterHost::on_internal_notification / IPC Subscription information found (ANY_EVENT)";

            subscribe_list_ptr = &(serviceinfo->eventinfos[SOMEIP_DEFAULT_ANY_EVENT]);
        }
    } else {
        LGSOMEIP_LOG_DEBUG << "PacketRouterHost::on_internal_notification / Subscription information found";

        subscribe_list_ptr = &(subscribe_list_it->second);
    }

    auto& subscribelist = *subscribe_list_ptr;

    // for notify_one
    if (target_client > 0) {
        request_id = (request_id & 0x0000FFFF);
        set_byte_stream(message + SOMEIP_HEADER::POS::REQUESTID, &request_id);
        bool is_sent = false;

        for (auto& recv : subscribelist) {
            if (recv.app_id == target_client) {
                // Add instanceID in the end of the message if this message is for internal
                std::uint16_t instance_id = endpoint->get_instance_id();
                set_byte_stream(message + message_length, &instance_id, 2);

                recv.endpoint->send_message(message, message_length + 2);

                LGSOMEIP_LOG_DEBUG << "PacketRouterHost::on_internal_notification / " << "target "
                                   << format_named_id("AppID", recv.app_id, 4);

                is_sent = true;
            }
        }

        if (is_sent == true) {
#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
            SomeipPacketStatistics::get_instance().increase_delivered_packet(SomeipPacketStatistics::kOutgoing,
                                                                             message_id);
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS
        } else {
            LGSOMEIP_LOG_DEBUG << "PacketRouterHost::on_internal_notification "
                               << format_service_instance_event_id(serviceid, endpoint->get_instance_id(), eventid)
                               << " " << format_named_id("RequestID", original_request_id, 8)
                               << " There is no subscriber for target " << format_named_id("AppID", target_client, 4);

#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
            SomeipPacketStatistics::get_instance().increase_dropped_packet(
                SomeipPacketStatistics::kOutgoing, message_id, SomeipPacketStatistics::kPacketRouterHostNoTargetClient);
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS
        }

        return;
    }

    bool is_sent = false;

    // When multicast event is enabled
    if (serviceinfo->multicasts.size() > 0) {
        std::uint16_t instanceid = find_instance_id(serviceid, endpoint);
        struct AvailableService* available_service_info = host_->find_available_service_instance(serviceid, instanceid);

        auto service_config = host_->get_configuration()->get_service_info(serviceid, instanceid);
        auto eventgroups = service_config->get_events()->find(eventid);
        if (eventgroups != service_config->get_events()->end()) {
            std::set<std::shared_ptr<lgsomeip::osabstraction::Address>> multicastlist;

            for (auto eventgroupid : eventgroups->second) {
                auto eventgroup_object = service_config->get_event_group_object(eventgroupid);
                bool is_multicast = eventgroup_object->is_multicast();
                if (is_multicast == false)
                    continue;
                auto threshold = eventgroup_object->get_threshold();
                auto& available_list = available_service_info->subscribe[eventgroupid];

                if (available_list.size() >= threshold) {
                    multicastlist.insert(eventgroup_object->get_multicast_address());
                    multicast_eventgroups.push_back(eventgroupid);
                }
            }

            for (auto iter = multicastlist.begin(); iter != multicastlist.end(); iter++) {
                if (*iter == nullptr) {
                    auto multiaddr = (*(service_config->get_multicast_address()))[0];
                    if (multiaddr != nullptr && service_discovery_unicast_ != nullptr) {
                        service_discovery_unicast_->send_message(message, message_length, multiaddr);

                        LGSOMEIP_LOG_DEBUG << "PacketRouterHost::on_internal_notification / send multicast to "
                                           << multiaddr->to_string();

                        is_sent = true;
                    }
                } else if (service_discovery_unicast_ != nullptr) {
                    service_discovery_unicast_->send_message(message, message_length, *iter);

                    LGSOMEIP_LOG_DEBUG << "PacketRouterHost::on_internal_notification / send multicast to "
                                       << (*iter)->to_string();
                }
            }
        }
    }

    for (auto& recv : subscribelist) {
        if (recv.app_id > 0) {
            // Add instanceID in the end of the message if this message is for internal
            std::uint16_t instance_id = endpoint->get_instance_id();
            set_byte_stream(message + message_length, &instance_id, 2);

            recv.endpoint->send_message(message, message_length + 2);

            LGSOMEIP_LOG_DEBUG << "PacketRouterHost::on_internal_notification / " << "target "
                               << format_named_id("AppID", recv.app_id, 4);

            is_sent = true;
        } else {
            if (multicast_eventgroups.size() > 0) {
                bool multicast_enabled = false;
                for (auto eventgroupid : multicast_eventgroups) {
                    if (recv.eventgroupid == eventgroupid) {
                        multicast_enabled = true;
                        break;
                    }
                }
                if (multicast_enabled)
                    continue;
            }

            auto dstaddr = recv.endpoint->get_socket()->get_dst_address();
            if (recv.destAddr != nullptr)
                dstaddr = recv.destAddr;
            LGSOMEIP_LOG_DEBUG << "PacketRouterHost::on_internal_notification / target addr:" << dstaddr->to_string();

            if (recv.destAddr != nullptr) {
                recv.endpoint->send_message(message, message_length, recv.destAddr);
            } else {
                recv.endpoint->send_message(message, message_length);
            }

            is_sent = true;
        }
    }

    if (is_sent == true) {
#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
        SomeipPacketStatistics::get_instance().increase_delivered_packet(SomeipPacketStatistics::kOutgoing, message_id);
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS
    } else {
        LGSOMEIP_LOG_DEBUG << "PacketRouterHost::on_internal_notification "
                           << format_service_instance_event_id(serviceid, endpoint->get_instance_id(), eventid) << " "
                           << format_named_id("RequestID", original_request_id, 8)
                           << " There is no subscriber for the Notification";

#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
        SomeipPacketStatistics::get_instance().increase_dropped_packet(
            SomeipPacketStatistics::kOutgoing, message_id, SomeipPacketStatistics::kPacketRouterHostNoSubscription);
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS
    }
}

void PacketRouterHost::send_error(std::shared_ptr<Endpoint> endpoint, std::uint8_t* message, std::uint32_t message_id,
                                  std::uint8_t return_code) {
    std::uint8_t type;
    std::uint32_t length = 8;
    std::uint8_t msg[16];

    if (message == nullptr)
        return;
    get_byte_stream(&type, message + SOMEIP_HEADER::POS::MESSAGETYPE);
    if (type != SOMEIP_MESSAGE_TYPE::REQUEST)
        return;

    memcpy(msg, message, SOMEIP_HEADER::SIZE - 2); // except Message Type, Return Code

    type = SOMEIP_MESSAGE_TYPE::ERROR;
    set_byte_stream(msg + SOMEIP_HEADER::POS::MESSAGETYPE, &type);
    set_byte_stream(msg + SOMEIP_HEADER::POS::RETURNCODE, &return_code);
    set_byte_stream(msg + SOMEIP_HEADER::POS::LENGTH, &length); // Only SOMEIP_HEADER

    if (endpoint != nullptr) {
        LGSOMEIP_LOG_INFO << "PacketRouterHost::SendError with return code " << static_cast<uint32_t>(return_code);

        length = SOMEIP_HEADER::SIZE;

        if ((endpoint.get() != nullptr) && (endpoint->get_socket().get() == nullptr)) {
            std::uint16_t instance_id = find_instance_id((std::uint16_t)(message_id >> 16), endpoint);
            set_byte_stream(msg + length, &instance_id, 2);
            length += 2;

            endpoint->send_message(msg, length);
        } else {
            if (endpoint->get_sender_address()->get_type() == AF_UNIX) {
                // Add instanceID in the end of the message if this message is destined for internal
                std::uint16_t instance_id = find_instance_id((std::uint16_t)(message_id >> 16), endpoint);
                set_byte_stream(msg + length, &instance_id, 2);
                length += 2;
            }

            if (endpoint->get_socket()->get_reliable() == false) {
                endpoint->send_message(msg, length, endpoint->get_sender_address());
            } else {
                endpoint->send_message(msg, length);
            }
        }
    }
}

// -----------------------------------------------------------------------------
//  PacketRouter Host : External / Callback for Data Message
// -----------------------------------------------------------------------------
/*
    The UDP datagram size shall be at least 16 Bytes (minimum size of a SOME/IP message).
    The value of the length field shall be less than or equal to the remaining bytes in the UDP datagram payload.

    SOME/IP messages shall be checked by error processing. This does not include
    the application based error handling but just covers the error handling in messaging and RPC.
*/
void PacketRouterHost::on_external_message(std::shared_ptr<Endpoint> endpoint, std::uint8_t* message,
                                           std::size_t message_length) {
    MessageHeader header;
    try {
        bool success = header.deserialize(message, message_length);
        if (!success) {
            throw LSAR_APPLICATION_ERROR(SOMEIP_RETURN_CODE::E_MALFORMED_MESSAGE);
        }

        // The first condition (UDP datagram size >= 16 bytes) will be checked in deserialize() right above.
        if (endpoint->get_socket()->get_reliable() == false) {
            // Remaining length from length field to the end of SOME/IP header (8 bytes)
            static const std::uint32_t rest_hdr_len_from_length =
                SOMEIP_HEADER::SIZE - SOMEIP_HEADER::ACCUM_LEN::LENGTH;
            std::uint32_t length;
            get_byte_stream(&length, message + SOMEIP_HEADER::POS::LENGTH);
            if (length < rest_hdr_len_from_length || length > message_length) {
                throw LSAR_APPLICATION_ERROR(SOMEIP_RETURN_CODE::E_MALFORMED_MESSAGE);
            }
        }

        std::uint8_t type = header.get_message_type();
        std::uint32_t messageid = header.get_message_id();
        std::uint32_t requestid = header.get_request_id();
        std::uint8_t protocol = header.get_protocol_version();
        if (protocol != 0x01) {
            throw LSAR_APPLICATION_ERROR(SOMEIP_RETURN_CODE::E_WRONG_PROTOCOL_VERSION);
        }

        auto configuration = host_->get_configuration();
        if (configuration->is_e2e_enabled()) {
            std::uint16_t serviceid = (std::uint16_t)(messageid >> 16);
            std::uint16_t methodid = (std::uint16_t)(messageid & 0xffff);
            std::pair<std::uint16_t, std::uint16_t> identifier = {serviceid, methodid};

            auto it = e2e_custom_checkers_.find(identifier);
            if (it != e2e_custom_checkers_.end()) {
                vsomeip::e2e::profile_interface::generic_check_status check_status;
                vsomeip::e2e_buffer input_buffer(message + SOMEIP_HEADER::SIZE, message + message_length);
                (it->second)->check(input_buffer, check_status);

                if (check_status != vsomeip::e2e::profile_interface::generic_check_status::E2E_OK) {
                    LGSOMEIP_LOG_INFO << "PacketRouterHost::on_external_message / E2E CRC check failed "
                                      << format_named_id("ServiceID", serviceid, 4) << " "
                                      << format_named_id("MethodID", methodid, 4) << " "
                                      << format_named_id("RequestID", requestid, 8);
                    uint8_t return_code = 0x20; // for vaild_crc
                    set_byte_stream(message + SOMEIP_HEADER::POS::RETURNCODE, &return_code);
                } else {
                    LGSOMEIP_LOG_INFO << "PacketRouterHost::on_external_message / E2E CRC check passed "
                                      << format_named_id("ServiceID", serviceid, 4) << " "
                                      << format_named_id("MethodID", methodid, 4) << " "
                                      << format_named_id("RequestID", requestid, 8);
                }
            }
        }

        switch (type) {
        case SOMEIP_MESSAGE_TYPE::REQUEST_NO_RETURN: // FALL-THROUGH
        case SOMEIP_MESSAGE_TYPE::TP_REQUEST_NO_RETURN:
            on_external_request(endpoint, message, message_length, messageid, requestid, true);
            break;
        case SOMEIP_MESSAGE_TYPE::REQUEST: // FALL-THROUGH
        case SOMEIP_MESSAGE_TYPE::TP_REQUEST:
            on_external_request(endpoint, message, message_length, messageid, requestid);
            break;
        case SOMEIP_MESSAGE_TYPE::RESPONSE:    // FALL-THROUGH
        case SOMEIP_MESSAGE_TYPE::TP_RESPONSE: // FALL-THROUGH
        case SOMEIP_MESSAGE_TYPE::ERROR:       // FALL-THROUGH
        case SOMEIP_MESSAGE_TYPE::TP_ERROR:
            on_external_response(endpoint, message, message_length, messageid, requestid);
            break;
        case SOMEIP_MESSAGE_TYPE::NOTIFICATION: // FALL-THROUGH
        case SOMEIP_MESSAGE_TYPE::TP_NOTIFICATION:
            on_external_notification(endpoint, message, message_length, messageid, requestid);
            break;
        default:
            LGSOMEIP_LOG_DEBUG << "PacketRouterHost::on_external_message unknown message type";
            type = SOMEIP_MESSAGE_TYPE::REQUEST;
            set_byte_stream(message + SOMEIP_HEADER::POS::MESSAGETYPE, &type);
            throw LSAR_APPLICATION_ERROR(SOMEIP_RETURN_CODE::E_WRONG_MESSAGE_TYPE);
            break;
        }
    } catch (const ApplicationErrorException& e) {
        // - If received datagram size is less than SOME/IP header size (16 byte),
        // DO NOT respond it with SOME/IP error message.
        // - Whether message type is Request or not will be checked in send_error().
        if (message_length > SOMEIP_HEADER::SIZE - 1) {
            send_error(endpoint, message, header.get_message_id(), e.get_error_code());
        }

#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
        SomeipPacketStatistics::get_instance().increase_dropped_packet(
            SomeipPacketStatistics::kIncoming, header.get_message_id(),
            SomeipPacketStatistics::kPacketRouterHostOnExternalMessageError);
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS
    }
}

void PacketRouterHost::on_external_request(std::shared_ptr<Endpoint> endpoint, std::uint8_t* message,
                                           std::size_t message_length, std::uint32_t message_id,
                                           std::uint32_t request_id, bool no_response) {
    std::uint16_t sid = (std::uint16_t)(message_id >> 16);
    std::uint16_t iid = (std::uint16_t)(message_id & 0xffff);
    LGSOMEIP_LOG_DEBUG << "PacketRouterHost::on_external_request "
                       << format_service_instance_event_id(sid, endpoint->get_instance_id(), iid) << " "
                       << format_named_id("RequestID", request_id, 8);

    std::uint8_t return_code;
    get_byte_stream(&return_code, message + SOMEIP_HEADER::POS::RETURNCODE);
    if (return_code >= 0x01 && return_code <= 0x1f) {
        // if the message is request and it has return code (0x01 ~ 0x1f),
        // then just ignore the message (do not response)
        LGSOMEIP_LOG_WARN << "PacketRouterHost::on_external_request "
                          << format_service_instance_event_id(sid, endpoint->get_instance_id(), iid) << " "
                          << format_named_id("RequestID", request_id, 8) << " return code (" << return_code
                          << ") is not 0";

#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
        SomeipPacketStatistics::get_instance().increase_dropped_packet(
            SomeipPacketStatistics::kIncoming, message_id, SomeipPacketStatistics::kPacketRouterHostInvalidReturnCode);
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS

        return;
    }

    struct RoutingServiceInfo* serviceinfo = get_service_instance(sid, endpoint);
    if (serviceinfo == nullptr) {
        throw LSAR_APPLICATION_ERROR(SOMEIP_RETURN_CODE::E_UNKNOWN_SERVICE);
    }

    if (serviceinfo->svcendpoint != nullptr) {
        std::uint32_t length;
        get_byte_stream(&length, message + SOMEIP_HEADER::POS::LENGTH);
        LGSOMEIP_LOG_DEBUG << "PacketRouterHost::on_external_request / "
                           << format_named_id("AppID", serviceinfo->app_id, 4)
                           << " message length: " << static_cast<std::uint32_t>(length + 8);

        if (no_response == false) {
            std::uint16_t clientid = static_cast<std::uint16_t>(request_id >> 16);
            auto& info = request_map_[clientid];
            info.ttl = 5;
            info.endpoint = endpoint;
            if (endpoint->get_socket()->get_reliable() == false) {
                info.targeaddr = endpoint->get_sender_address();
            } else {
                info.targeaddr = endpoint->get_socket()->get_dst_address();
            }
        }

        // Add instanceID in the end of the message if this message is destined for internal
        std::uint16_t instance_id = endpoint->get_instance_id();
        set_byte_stream(message + length + 8, &instance_id, 2);
        length += 2;

        serviceinfo->svcendpoint->send_message(message, length + 8);

#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
        SomeipPacketStatistics::get_instance().increase_delivered_packet(SomeipPacketStatistics::kIncoming, message_id);
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS
    }
}

void PacketRouterHost::on_external_response(std::shared_ptr<Endpoint> endpoint, std::uint8_t* message,
                                            std::size_t message_length, std::uint32_t message_id,
                                            std::uint32_t request_id) {
    std::uint16_t clientid = static_cast<std::uint16_t>(request_id >> 16);
    if (request_map_.find(clientid) == request_map_.end()) {
        const std::uint16_t sid = static_cast<std::uint16_t>(message_id >> 16);
        LGSOMEIP_LOG_INFO << "PacketRouterHost::on_external_response / " << format_named_id("ServiceID", sid, 4) << " "
                          << format_named_id("RequestID", request_id, 8) << " - skipped(client not found)";

#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
        SomeipPacketStatistics::get_instance().increase_dropped_packet(
            SomeipPacketStatistics::kIncoming, message_id,
            SomeipPacketStatistics::kPacketRouterHostNoRequestForResponse);
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS

        return;
    }

    auto& info = request_map_[clientid];
    auto& request_endpoint = info.endpoint;

    if (request_endpoint != nullptr) {
        // Add instanceID in the end of the message if this message is destined for internal
        std::uint16_t instance_id = find_instance_id((std::uint16_t)(message_id >> 16), request_endpoint);
        set_byte_stream(message + message_length, &instance_id, 2);
        message_length += 2;

        request_endpoint->send_message(message, message_length);

#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
        SomeipPacketStatistics::get_instance().increase_delivered_packet(SomeipPacketStatistics::kIncoming, message_id);
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS
    } else {
        const std::uint16_t sid = static_cast<std::uint16_t>(message_id >> 16);
        LGSOMEIP_LOG_WARN << "PacketRouterHost::on_external_response / " << format_named_id("ServiceID", sid, 4) << " "
                          << format_named_id("RequestID", request_id, 8)
                          << " - There is no EndPoint for this response.";

#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
        SomeipPacketStatistics::get_instance().increase_dropped_packet(
            SomeipPacketStatistics::kIncoming, message_id, SomeipPacketStatistics::kPacketRouterHostNoEndPoint);
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS
    }
}

void PacketRouterHost::on_external_notification(std::shared_ptr<Endpoint> endpoint, std::uint8_t* message,
                                                std::size_t message_length, std::uint32_t message_id,
                                                std::uint32_t request_id, std::uint8_t retry_count) {
    std::uint16_t sid = (std::uint16_t)(message_id >> 16);
    std::uint16_t eventid = (std::uint16_t)(message_id & 0xffff);

    LGSOMEIP_LOG_DEBUG << "PacketRouterHost::on_external_notification "
                       << format_service_instance_event_id(sid, endpoint->get_instance_id(), eventid) << " "
                       << format_named_id("RequestID", request_id, 8);

    struct RoutingServiceInfo* serviceinfo = get_service_instance(sid, endpoint);
    if (serviceinfo == nullptr) {
        LGSOMEIP_LOG_DEBUG << "PacketRouterHost::on_external_notification "
                           << format_service_instance_event_id(sid, endpoint->get_instance_id(), eventid) << " "
                           << format_named_id("RequestID", request_id, 8) << " No Service Information";

#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
        SomeipPacketStatistics::get_instance().increase_dropped_packet(
            SomeipPacketStatistics::kIncoming, message_id, SomeipPacketStatistics::kPacketRouterHostNoServiceInfo);
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS

        return;
    }

    auto subscribe_list_it = serviceinfo->eventinfos.find(eventid);
    if (subscribe_list_it == serviceinfo->eventinfos.end()) {
        LGSOMEIP_LOG_DEBUG << "PacketRouterHost::on_external_notification "
                           << format_service_instance_event_id(sid, endpoint->get_instance_id(), eventid) << " "
                           << format_named_id("RequestID", request_id, 8) << " No subscription information";

        // Check if the subscription of the service for this event has already been sent.
        struct AvailableService* available_service_info =
            host_->find_available_service_instance(sid, endpoint->get_instance_id());
        auto service_config = host_->get_configuration()->get_service_info(sid, endpoint->get_instance_id());

        bool is_subscribe_sent = false;

        if (service_config != nullptr && available_service_info != nullptr) {
            LGSOMEIP_LOG_INFO << "PacketRouterHost::on_external_notification / Found service in availableDB and config";
            auto eventgroups = service_config->get_events()->find(eventid);
            if (eventgroups != service_config->get_events()->end()) {
                LGSOMEIP_LOG_INFO
                    << "PacketRouterHost::on_external_notification / Found the event among all eventgroups";
                for (auto eventgroupid : eventgroups->second) {
                    auto& available_list = available_service_info->subscribe[eventgroupid];
                    LGSOMEIP_LOG_INFO << "PacketRouterHost::on_external_notification / "
                                      << format_named_id("EventGroupID", eventgroupid, 4)
                                      << ", Size of subscribers: " << available_list.size();
                    if (available_list.size() > 0) {
                        is_subscribe_sent = true;
                        break;
                    }
                }
            }
        }

        // To ensure delivering the initial value of a service which has been received
        // before the SubscribeEventgroupAck is received,
        // this function (on_external_notification) will be called again with the initial value.
        // Retrying will be attempted twice.
        if (retry_count < 2 && is_subscribe_sent == true) {
            LGSOMEIP_LOG_INFO << "PacketRouterHost::on_external_notification "
                              << format_service_instance_event_id(sid, endpoint->get_instance_id(), eventid) << " "
                              << format_named_id("RequestID", request_id, 8) << ", retry_count: " << retry_count
                              << " This event is going to be re-transmitted in 50ms.";

            // Keep data of the message pointer in the local variable, which will be used in the
            // retransmitNotificationTh thread
            std::vector<uint8_t> temp_msg(message, message + message_length);

            std::thread retransmit_notification_th([=]() {
                // For assigning thread name
                pthread_setname_np(pthread_self(), "SomeipRetranNoti");

                std::shared_ptr<Endpoint> retrans_endpoint = endpoint;
                std::vector<uint8_t> retrans_msg(temp_msg);
                std::size_t retrans_msg_len = message_length;
                std::uint8_t retry_counter = retry_count;

                // wait for SubscribeEventgroupAck to be processed
                std::this_thread::sleep_for(std::chrono::milliseconds(50));

#if defined(ENABLE_QNX_MESSAGE_PASSING) || defined(ENABLE_TLS)
                std::lock_guard<std::mutex> lock(mutex_processing_);
#endif
                // Call this function again to ensure that the initial value is delivered.
                on_external_notification(retrans_endpoint, retrans_msg.data(), retrans_msg_len, message_id, request_id,
                                         ++retry_counter);
            });

            retransmit_notification_th.detach();
        } else {
            LGSOMEIP_LOG_DEBUG << "PacketRouterHost::on_external_notification "
                               << format_service_instance_event_id(sid, endpoint->get_instance_id(), eventid) << " "
                               << format_named_id("RequestID", request_id, 8) << ", retry_count: " << retry_count
                               << " This event will be dropped."
                               << (is_subscribe_sent ? "(retry_count => 2)" : "(The subscribe has not been sent.)");

#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
            SomeipPacketStatistics::get_instance().increase_dropped_packet(
                SomeipPacketStatistics::kIncoming, message_id,
                SomeipPacketStatistics::kPacketRouterHostNoSubscriptionInfo);
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS
        }

        return;
    }

    bool is_sent = false;
    auto& subscribelist = subscribe_list_it->second;
    for (auto& recv : subscribelist) {
        LGSOMEIP_LOG_DEBUG << "PacketRouterHost::on_external_notification / send to "
                           << format_named_id("AppID", recv.app_id, 4);

        if (recv.app_id > 0) {
            // Add instanceID in the end of the message if this message is destined for internal
            std::uint16_t instance_id = endpoint->get_instance_id();
            set_byte_stream(message + message_length, &instance_id, 2);
            recv.endpoint->send_message(message, message_length + 2);

            is_sent = true;
        }
    }

    if (is_sent == true) {
#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
        SomeipPacketStatistics::get_instance().increase_delivered_packet(SomeipPacketStatistics::kIncoming, message_id);
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS
    } else {
        LGSOMEIP_LOG_INFO << "PacketRouterHost::on_external_notification "
                          << format_service_instance_event_id(sid, endpoint->get_instance_id(), eventid) << " "
                          << format_named_id("RequestID", request_id, 8)
                          << " There is no subscriber for the notification.";

#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
        SomeipPacketStatistics::get_instance().increase_dropped_packet(
            SomeipPacketStatistics::kIncoming, message_id, SomeipPacketStatistics::kPacketRouterHostNoSubscription);
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS
    }
}

std::shared_ptr<Endpoint>
PacketRouterHost::find_endpoint(std::shared_ptr<lgsomeip::osabstraction::Address> source_address,
                                std::shared_ptr<lgsomeip::osabstraction::Address> destination_address) {
    std::shared_ptr<Endpoint> found_ep = nullptr;

    std::lock_guard<std::recursive_mutex> guard(route_management_mutex_);

    for (auto& element : connected_endpoints_) {
        auto cmp_ep = element.second.endpoint;
        if (cmp_ep == nullptr)
            continue;

        auto cmp_src_addr = cmp_ep->get_socket()->get_src_address();
        auto cmp_dst_addr = cmp_ep->get_socket()->get_dst_address();
        bool cmp_result = false;

        if (cmp_src_addr != nullptr) {
            cmp_result = (*cmp_src_addr == *source_address);

            if (cmp_dst_addr != nullptr && destination_address != nullptr) {
                cmp_result = cmp_result && (*cmp_dst_addr == *destination_address);
            }
        } else {
            if (cmp_dst_addr != nullptr && destination_address != nullptr) {
                cmp_result = (*cmp_dst_addr == *destination_address);
            }
        }

        if (cmp_result == true) {
            found_ep = cmp_ep;
            break;
        }
    }

    return found_ep;
}

// -----------------------------------------------------------------------------
//  PacketRouter Host : Service Control Opertion
// -----------------------------------------------------------------------------
bool PacketRouterHost::add_route(
    std::uint16_t service_id, std::uint16_t instance_id, std::uint16_t app_id,
    std::shared_ptr<lgsomeip::osabstraction::Address> remote_tcp_address, std::uint16_t local_tcp_port,
    std::shared_ptr<lgsomeip::osabstraction::Address> remote_udp_address, std::uint16_t local_udp_port,
    std::uint8_t vlan_priority,
    std::shared_ptr<std::vector<std::shared_ptr<lgsomeip::osabstraction::Address>>> local_multicast_list) {
    auto service_info = host_->get_configuration()->get_service_info(service_id, instance_id);
    bool is_secure_connection = false;
    if (service_info) {
        is_secure_connection = service_info->is_secure_connection();
    }

    bool is_offered_from_internal = (app_id > 0);
    bool is_ipc_service = (is_offered_from_internal && (service_info == nullptr));
    bool ret = false;

    if (is_ipc_service) {
        LGSOMEIP_LOG_DEBUG << "PacketRouterHost::add_route / New IPC service "
                           << format_service_instance_id(service_id, instance_id) << " Provided by "
                           << format_named_id("AppID", app_id, 4);
        std::lock_guard<std::recursive_mutex> guard(route_management_mutex_);

        registered_service_info_[service_id][instance_id].app_id = app_id;
        registered_service_info_[service_id][instance_id].svcendpoint = local_applications_[app_id].sender;
        set_instance_id(service_id, instance_id, app_id);
        ret = true;
    } else {
        std::shared_ptr<lgsomeip::osabstraction::Address> local_tcp_addr = nullptr;
        std::shared_ptr<lgsomeip::osabstraction::Address> local_udp_addr = nullptr;

        if (local_tcp_port != 0) {
            local_tcp_addr = make_address(true, local_tcp_port, vlan_priority);
        }

        if (local_udp_port != 0) {
            local_udp_addr = make_address(false, local_udp_port, vlan_priority);
        }

        if (is_offered_from_internal) {
            ret = add_new_server_route(service_id, instance_id, app_id, local_tcp_addr, local_udp_addr,
                                       local_multicast_list, is_secure_connection);
        } else {
            ret = add_new_client_route(service_id, instance_id, remote_tcp_address, local_tcp_addr, remote_udp_address,
                                       local_udp_addr, is_secure_connection);
        }
    }

    return ret;
}

bool PacketRouterHost::add_new_server_route(
    std::uint16_t service_id, std::uint16_t instance_id, std::uint16_t app_id,
    std::shared_ptr<lgsomeip::osabstraction::Address> local_tcp_address,
    std::shared_ptr<lgsomeip::osabstraction::Address> local_udp_address,
    std::shared_ptr<std::vector<std::shared_ptr<lgsomeip::osabstraction::Address>>> local_multicast_list,
    bool secure_connection) {
    std::shared_ptr<Endpoint> tcp_ep = nullptr;
    std::shared_ptr<Endpoint> udp_ep = nullptr;

    // Find or create TCP endpoint. If it fails do not save information and return false.
    if (local_tcp_address != nullptr) {
        tcp_ep = find_or_create_server_endpoint(local_tcp_address, secure_connection);

        if (tcp_ep == nullptr) {
            LGSOMEIP_LOG_ERROR << "PacketRouterHost::add_new_server_route / Endpoint cannot be created!! TCP Port = "
                               << local_tcp_address->get_port_address();

            // Remove TCP address from ongoing_add_routes_.
            remove_ongoing_add_route(local_tcp_address);
            return false;
        }
    }

    // Find or create UDP endpoint. If it fails do not save information and return false.
    if (local_udp_address != nullptr) {
        udp_ep = find_or_create_server_endpoint(local_udp_address, secure_connection);

        if (udp_ep == nullptr) {
            LGSOMEIP_LOG_ERROR << "PacketRouterHost::add_new_server_route / Endpoint cannot be created!! UDP Port = "
                               << local_udp_address->get_port_address();

            // Remove UDP address (and TCP address if exists) from ongoing_add_routes_.
            remove_ongoing_add_route(local_udp_address);
            if (local_tcp_address != nullptr)
                remove_ongoing_add_route(local_tcp_address);
            return false;
        }
    }

    // Find or create multicast endpoint. If one of creation fails do not save information and return false.
    std::map<std::string, std::shared_ptr<Endpoint>> registered_multicast;
    if (local_multicast_list != nullptr) {
        for (auto multicast_it = (*local_multicast_list).begin(); multicast_it != (*local_multicast_list).end();
             ++multicast_it) {
            auto& multicast_addr = *multicast_it;
            std::string multicast_ip_addr =
                multicast_addr->get_ip_address() + ":" + std::to_string(multicast_addr->get_port_address());

            // It is possible that same multicast addresses is in local_multicast_list,
            // since same multicast address can be assigned by different eventgroup.
            // In this case just increase refCount of endpoint.
            auto find_it = registered_multicast.find(multicast_ip_addr);
            if (find_it == registered_multicast.end()) {
                std::shared_ptr<Endpoint> multicast_ep = find_or_create_multicast_endpoint(multicast_addr);

                if (multicast_ep != nullptr) {
                    registered_multicast[multicast_ip_addr] = multicast_ep;
                } else {
                    LGSOMEIP_LOG_ERROR
                        << "PacketRouterHost::add_new_server_route / Endpoint cannot be created!! Multicast Port = "
                        << multicast_addr->get_port_address();

                    // Remove multicast address handled so far from ongoing_add_routes_.
                    // Also remove TCP and UDP addr if exist.
                    for (auto remove_it = (*local_multicast_list).begin(); remove_it != multicast_it; ++remove_it) {
                        remove_ongoing_add_route(*remove_it);
                    }
                    if (local_tcp_address != nullptr)
                        remove_ongoing_add_route(local_tcp_address);
                    if (local_udp_address != nullptr)
                        remove_ongoing_add_route(local_udp_address);

                    return false;
                }
            } else {
                LGSOMEIP_LOG_DEBUG
                    << "PacketRouterHost::add_new_server_route / Endpoint already created in this session,"
                    << "with Multicast Addr:" << multicast_ip_addr;

                std::shared_ptr<Endpoint> multicast_ep = find_it->second;
                multicast_ep->increase_reference_count();
            }
        }
    }

    // If all endpoints are created successfully save them into DB.
    std::lock_guard<std::recursive_mutex> guard(route_management_mutex_);

    registered_service_info_[service_id][instance_id].app_id = app_id;
    registered_service_info_[service_id][instance_id].svcendpoint = local_applications_[app_id].sender;
    set_instance_id(service_id, instance_id, app_id);
    if (tcp_ep != nullptr) {
        LGSOMEIP_LOG_DEBUG << "PacketRouterHost::add_new_server_route / "
                           << format_service_instance_id(service_id, instance_id)
                           << " Add route with TCP Port=" << local_tcp_address->get_port_address() << ", Provided by "
                           << format_named_id("AppID", app_id, 4);
        register_connected_endpoint(tcp_ep, app_id);

        registered_service_info_[service_id][instance_id].tcpendpoint = tcp_ep;
        set_instance_id(service_id, instance_id, local_tcp_address);
        remove_ongoing_add_route(local_tcp_address);

        auto service_info = host_->get_configuration()->get_service_info(service_id, instance_id);
        if (service_info != nullptr && service_info->get_state_magic_cookies() == true) {
            LGSOMEIP_LOG_DEBUG << "PacketRouterHost::add_new_server_route / Magic cookie(server) enabled on our port ="
                               << local_tcp_address->get_port_address();
            tcp_ep->set_magic_cookie_enabled(true);
        }
    }

    if (udp_ep != nullptr) {
        LGSOMEIP_LOG_DEBUG << "PacketRouterHost::add_new_server_route / "
                           << format_service_instance_id(service_id, instance_id)
                           << " Add route with UDP Port=" << local_udp_address->get_port_address() << ", Provided by "
                           << format_named_id("AppID", app_id, 4);
        register_connected_endpoint(udp_ep, app_id);

        registered_service_info_[service_id][instance_id].udpendpoint = udp_ep;
        set_instance_id(service_id, instance_id, local_udp_address);
        remove_ongoing_add_route(local_udp_address);
    }

    if (registered_multicast.empty() == false) {
        for (auto& multicast : registered_multicast) {
            std::shared_ptr<Endpoint> multicast_ep = multicast.second;
            std::shared_ptr<lgsomeip::osabstraction::Address> multicast_addr =
                multicast_ep->get_socket()->get_src_address();
            LGSOMEIP_LOG_DEBUG << "PacketRouterHost::add_new_server_route / "
                               << format_service_instance_id(service_id, instance_id)
                               << " Add route with Multicast Port=" << multicast_addr->get_port_address()
                               << ", Provided by " << format_named_id("AppID", app_id, 4);
            register_connected_endpoint(multicast_ep, app_id);

            registered_service_info_[service_id][instance_id].multicasts.push_back(multicast_ep);
            set_instance_id(service_id, instance_id, multicast_addr);
            remove_ongoing_add_route(multicast_addr);
        }
    }

    return true;
}

bool PacketRouterHost::add_new_client_route(std::uint16_t service_id, std::uint16_t instance_id,
                                            std::shared_ptr<lgsomeip::osabstraction::Address> remote_tcp_address,
                                            std::shared_ptr<lgsomeip::osabstraction::Address> local_tcp_address,
                                            std::shared_ptr<lgsomeip::osabstraction::Address> remote_udp_address,
                                            std::shared_ptr<lgsomeip::osabstraction::Address> local_udp_address,
                                            bool secure_connection) {
    std::shared_ptr<Endpoint> tcp_ep = nullptr;
    std::shared_ptr<Endpoint> udp_ep = nullptr;

    // Find or create TCP endpoint. If it fails do not save information and return false.
    if (remote_tcp_address != nullptr && remote_tcp_address->get_reliable() == true && local_tcp_address) {
        tcp_ep = find_or_create_client_endpoint(local_tcp_address, remote_tcp_address, secure_connection);

        if (tcp_ep == nullptr) {
            LGSOMEIP_LOG_WARN << "PacketRouterHost::add_new_client_route / Endpoint cannot be created!! TCP Port = "
                              << local_tcp_address->get_port_address();

            // Remove TCP address from ongoing_add_routes_.
            remove_ongoing_add_route(local_tcp_address, remote_tcp_address);
            return false;
        }
    }

    // Find or create UDP endpoint. If it fails do not save information and return false.
    if (remote_udp_address != nullptr && remote_udp_address->get_reliable() == false && local_udp_address) {
        udp_ep = find_or_create_client_endpoint(local_udp_address, remote_udp_address, secure_connection);

        if (udp_ep == nullptr) {
            LGSOMEIP_LOG_WARN << "PacketRouterHost::add_new_client_route / Endpoint cannot be created!! UDP Port = "
                              << local_udp_address->get_port_address();

            // Remove UDP address (and TCP address if exists) from ongoing_add_routes_.
            remove_ongoing_add_route(local_udp_address);
            if (remote_tcp_address != nullptr && remote_tcp_address->get_reliable() == true) {
                remove_ongoing_add_route(local_tcp_address, remote_tcp_address);
            }
            return false;
        }
    }

    // If all endpoints are created successfully save them into DB.
    if (tcp_ep != nullptr) {
        LGSOMEIP_LOG_DEBUG << "PacketRouterHost::add_new_client_route / "
                           << format_service_instance_id(service_id, instance_id)
                           << " Add route with TCP Port=" << local_tcp_address->get_port_address()
                           << ", Remote Addr=" << remote_tcp_address->get_ip_address();
        register_connected_endpoint(tcp_ep);

        std::lock_guard<std::recursive_mutex> guard(route_management_mutex_);

        registered_service_info_[service_id][instance_id].tcpendpoint = tcp_ep;
        set_instance_id(service_id, instance_id, local_tcp_address);
        remove_ongoing_add_route(local_tcp_address, remote_tcp_address);

        auto service_info = host_->get_configuration()->get_service_info(service_id, instance_id);
        if (service_info != nullptr && service_info->get_state_magic_cookies() == true) {
            LGSOMEIP_LOG_DEBUG << "PacketRouterHost::add_new_client_route / Magic cookie(server) enabled on our port ="
                               << local_tcp_address->get_port_address();
            tcp_ep->set_magic_cookie_enabled(true);
        }
    }

    if (udp_ep != nullptr) {
        LGSOMEIP_LOG_DEBUG << "PacketRouterHost::add_new_client_route / "
                           << format_service_instance_id(service_id, instance_id)
                           << " Add route with UDP Port=" << local_udp_address->get_port_address()
                           << ", Remote Addr=" << remote_udp_address->get_ip_address();
        register_connected_endpoint(udp_ep);

        std::lock_guard<std::recursive_mutex> guard(route_management_mutex_);

        registered_service_info_[service_id][instance_id].udpendpoint = udp_ep;
        registered_service_info_[service_id][instance_id].udpserveraddress = remote_udp_address;
        set_instance_id(service_id, instance_id, local_udp_address);
        remove_ongoing_add_route(local_udp_address);
    }

    if (tcp_ep == nullptr && udp_ep == nullptr) {
        LGSOMEIP_LOG_WARN << "PacketRouterHost::add_new_client_route / Endpoint cannot be created!!";
        return false;
    }

    return true;
}

std::shared_ptr<Endpoint>
PacketRouterHost::find_or_create_server_endpoint(std::shared_ptr<lgsomeip::osabstraction::Address> address,
                                                 bool secure_connection) {
    std::uint16_t port = address->get_port_address();
    bool is_reliable = address->get_reliable();

    // Wait for other thread to connect same address. (blocking on a same address)
    wait_for_ongoing_add_route(address);

    std::shared_ptr<Endpoint> ep = find_endpoint(address);
    if (ep == nullptr) {
        ep = EndpointUtils::create_server_endpoint(this, address, secure_connection);
    } else {
        LGSOMEIP_LOG_DEBUG << "PacketRouterHost::find_or_create_server_endpoint / Use the existing port"
                           << "refCount=" << static_cast<std::uint16_t>(ep->get_reference_count())
                           << (is_reliable ? " , TCP " : ", UDP ") << "Port = " << port;
    }

    return ep;
}

std::shared_ptr<Endpoint>
PacketRouterHost::find_or_create_client_endpoint(std::shared_ptr<lgsomeip::osabstraction::Address> local_addr,
                                                 std::shared_ptr<lgsomeip::osabstraction::Address> remote_addr,
                                                 bool secure_connection) {
    bool is_reliable = remote_addr->get_reliable();
    std::shared_ptr<Endpoint> ep = nullptr;

    if (is_reliable) {
        // Wait for other thread to connect same address. (blocking on a same address)
        wait_for_ongoing_add_route(local_addr, remote_addr);
        ep = find_endpoint(local_addr, remote_addr);
    } else {
        // In case of UDP, it is enough to handle _localAddr endpoint only.
        // Wait for other thread to connect same address. (blocking on a same address)
        wait_for_ongoing_add_route(local_addr);
        ep = find_endpoint(local_addr);
    }

    if (ep == nullptr) {
        ep = EndpointUtils::create_client_endpoint(this, local_addr, remote_addr, secure_connection);
    } else {
        LGSOMEIP_LOG_DEBUG << "PacketRouterHost::find_or_create_client_endpoint / Use the existing port"
                           << "refCount=" << static_cast<std::uint16_t>(ep->get_reference_count())
                           << (is_reliable ? " , TCP " : ", UDP ") << "Port = " << local_addr->get_port_address();
    }

    return ep;
}

std::shared_ptr<Endpoint>
PacketRouterHost::find_or_create_multicast_endpoint(std::shared_ptr<lgsomeip::osabstraction::Address> address) {
    std::uint16_t port = address->get_port_address();

    // Wait for other thread to connect same address. (blocking on a same address)
    wait_for_ongoing_add_route(address);

    std::shared_ptr<Endpoint> ep = find_endpoint(address);

    if (ep == nullptr) {
        auto new_socket = std::make_shared<lgsomeip::osabstraction::UDPSocket>(address);
        new_socket->join_multicast(address->get_ip_address().c_str());
        new_socket->bind();
        ep = std::make_shared<EndpointUDP<PacketRouterHost>>(this);
        ep->set_socket(new_socket);
    } else {
        LGSOMEIP_LOG_DEBUG << "PacketRouterHost::find_or_create_multicast_endpoint / Use the existing port"
                           << " refCount=" << static_cast<std::uint16_t>(ep->get_reference_count())
                           << " , Multicast Port = " << port;
    }

    return ep;
}

void PacketRouterHost::wait_for_ongoing_add_route(
    const std::shared_ptr<lgsomeip::osabstraction::Address>& local_addr,
    const std::shared_ptr<lgsomeip::osabstraction::Address>& remote_addr) {
    std::uint16_t local_port = local_addr->get_port_address();
    std::string remote_key;
    if (remote_addr != nullptr) {
        std::stringstream ss;
        ss << remote_addr->get_ip_address() << ":" << remote_addr->get_port_address();
        remote_key = ss.str();
    }

    auto key = std::make_tuple(local_port, remote_key, local_addr->get_reliable());

    while (1) {
        std::unique_lock<std::mutex> lock(add_route_mutex_);

        // Check if local port / remote addr pair is in connection attempt.
        auto find_it = ongoing_add_routes_.find(key);
        if (find_it == ongoing_add_routes_.end()) {
            // If there's no progress, put it into DB and proceed to find endpoint.
            LGSOMEIP_LOG_DEBUG
                << "PacketRouterHost::wait_for_ongoing_add_route / Proceed to create endpoint for local port = "
                << local_port << (remote_key.empty() ? std::string() : ", remote Addr = " + remote_key);
            ongoing_add_routes_.emplace(key, local_addr->get_reliable());
            break;
        } else {
            LGSOMEIP_LOG_DEBUG
                << "PacketRouterHost::wait_for_ongoing_add_route / AddRoute already ongoing with same local Port = "
                << local_port << (remote_key.empty() ? std::string() : ", remote Addr = " + remote_key);
            lock.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
}

void PacketRouterHost::remove_ongoing_add_route(const std::shared_ptr<lgsomeip::osabstraction::Address>& local_addr,
                                                const std::shared_ptr<lgsomeip::osabstraction::Address>& remote_addr) {
    std::uint16_t local_port = local_addr->get_port_address();
    std::string remote_key;
    if (remote_addr != nullptr) {
        std::stringstream ss;
        ss << remote_addr->get_ip_address() << ":" << remote_addr->get_port_address();
        remote_key = ss.str();
    }

    auto key = std::make_tuple(local_port, remote_key, local_addr->get_reliable());

    std::lock_guard<std::mutex> lock(add_route_mutex_);
    auto find_it = ongoing_add_routes_.find(key);
    if (find_it != ongoing_add_routes_.end()) {
        LGSOMEIP_LOG_DEBUG
            << "PacketRouterHost::remove_ongoing_add_route / Endpoint handling finished with local Port = "
            << local_port << (remote_key.empty() ? std::string() : ", remote Addr = " + remote_key);

        ongoing_add_routes_.erase(find_it);
    }
}

void PacketRouterHost::remove_route(std::uint16_t service_id, std::uint16_t instance_id) {
    LGSOMEIP_LOG_DEBUG << "PacketRouterHost::remove_route / Remove Info "
                       << format_service_instance_id(service_id, instance_id);

    std::lock_guard<std::recursive_mutex> guard(route_management_mutex_);

    auto service_it = registered_service_info_.find(service_id);
    if (service_it == registered_service_info_.end()) {
        LGSOMEIP_LOG_DEBUG << "PacketRouterHost::remove_route / " << format_named_id("ServiceID", service_id, 4)
                           << " not found";
        return;
    }

    auto& instance_map = service_it->second;
    auto instance_it = instance_map.find(instance_id);
    if (instance_it == instance_map.end()) {
        LGSOMEIP_LOG_DEBUG << "PacketRouterHost::remove_route / " << format_named_id("InstanceID", instance_id, 4)
                           << " not found";
        return;
    }
    auto& service_info = instance_it->second;

    if (service_info.app_id > 0) {
        // remove TCP Client information
        auto tcp_endpoint = std::dynamic_pointer_cast<EndpointTCPServer<PacketRouterHost>>(service_info.tcpendpoint);
        if (tcp_endpoint != nullptr) {
            for (auto& client : tcp_endpoint->get_client_list()) {
                auto req_map_it = request_map_.begin();
                while (req_map_it != request_map_.end()) {
                    auto map_fd = req_map_it->second.endpoint->get_socket()->get_socket_fd();
                    if (map_fd == client) {
                        req_map_it = request_map_.erase(req_map_it);
                    } else {
                        ++req_map_it;
                    }
                }

                auto conn_info_it = connected_endpoints_.find(client);
                if (conn_info_it != connected_endpoints_.end()) {
                    auto& client_conn_info = conn_info_it->second;
                    client_conn_info.endpoint->decrease_reference_count();

                    LGSOMEIP_LOG_DEBUG << "PacketRouterHost::remove_route / referenceCount of this endpoint = "
                                       << static_cast<std::uint16_t>(client_conn_info.endpoint->get_reference_count())
                                       << ", " << format_named_id("AppID", service_info.app_id, 4);

                    if (client_conn_info.endpoint->get_reference_count() == 0) {
                        connected_endpoints_.erase(client);
                    }
                }
            }
        }
    }

    remove_connected_endpoint(service_info.tcpendpoint);
    remove_connected_endpoint(service_info.udpendpoint);
    auto& multicasts = service_info.multicasts;
    if (multicasts.size() > 0) {
        for (auto& multicast : multicasts) {
            remove_connected_endpoint(multicast, true);
        }
    }

    instance_map.erase(instance_id);
    if (registered_service_info_[service_id].empty()) {
        registered_service_info_.erase(service_id);
    }
}

void PacketRouterHost::remove_route(std::uint16_t app_id) {
    LGSOMEIP_LOG_DEBUG << "PacketRouterHost::remove_route / " << format_named_id("AppID", app_id, 4);

    std::lock_guard<std::recursive_mutex> guard(route_management_mutex_);

    for (auto service_it = registered_service_info_.begin(); service_it != registered_service_info_.end();) {
        auto& instance_map = service_it->second;
        for (auto instance_it = instance_map.begin(); instance_it != instance_map.end();) {
            auto& service_info = instance_it->second;
            std::uint16_t provider_app_id = service_info.app_id;

            // Remove route info offered by this app
            if (provider_app_id == app_id) {
                // remove TCP Client information
                auto tcp_endpoint =
                    std::dynamic_pointer_cast<EndpointTCPServer<PacketRouterHost>>(service_info.tcpendpoint);
                if (tcp_endpoint != nullptr) {
                    for (auto& client : tcp_endpoint->get_client_list()) {
                        auto req_map_it = request_map_.begin();
                        while (req_map_it != request_map_.end()) {
                            auto map_fd = req_map_it->second.endpoint->get_socket()->get_socket_fd();
                            if (map_fd == client)
                                req_map_it = request_map_.erase(req_map_it);
                            else
                                ++req_map_it;
                        }

                        auto conn_info_it = connected_endpoints_.find(client);
                        if (conn_info_it != connected_endpoints_.end()) {
                            auto& client_conn_info = conn_info_it->second;
                            client_conn_info.endpoint->decrease_reference_count();

                            LGSOMEIP_LOG_DEBUG
                                << "PacketRouterHost::remove_route / referenceCount of this endpoint = "
                                << static_cast<std::uint16_t>(client_conn_info.endpoint->get_reference_count())
                                << ", FD = " + client;

                            if (client_conn_info.endpoint->get_reference_count() == 0) {
                                connected_endpoints_.erase(client);
                            }
                        }
                    }
                }

                remove_connected_endpoint(service_info.tcpendpoint);
                remove_connected_endpoint(service_info.udpendpoint);
                auto& multicasts = service_info.multicasts;
                if (multicasts.size() > 0) {
                    for (auto& multicast : multicasts) {
                        remove_connected_endpoint(multicast, true);
                    }
                }

                LGSOMEIP_LOG_DEBUG << "PacketRouterHost::remove_route / remove route info "
                                   << format_service_instance_id(service_it->first, instance_it->first);
                instance_it = instance_map.erase(instance_it);
            }
            // Apply route cleanup when this app was the last subscriber to an external service.
            else if (provider_app_id == 0 && service_info.eventinfos.empty()) {
#if defined(REMOVE_ENDPOINT_DISCONNECTED_APP)
                // Remove the external route when enabled to release unused endpoints and stale route state;
                // otherwise, retain it for reconnecting applications.
                // No local subscription remains, so release the external transport endpoints.
                remove_connected_endpoint(serviceInfo.tcpendpoint);
                remove_connected_endpoint(serviceInfo.udpendpoint);
                auto& multicasts = serviceInfo.multicasts;
                if (multicasts.size() > 0) {
                    for (auto& multicast : multicasts) {
                        remove_connected_endpoint(multicast, true);
                    }
                }

                LGSOMEIP_LOG_DEBUG << "PacketRouterHost::remove_route / remove route info "
                                   << format_service_instance_id(serviceIt->first, instanceIt->first);
                instanceIt = instanceMap.erase(instanceIt);
#else
                // Retain the route so a restarted application can reuse it.
                ++instance_it;
#endif
            } else {
                ++instance_it;
            }
        }

        if (instance_map.empty()) {
            service_it = registered_service_info_.erase(service_it);
        } else {
            ++service_it;
        }
    }
}

void PacketRouterHost::remove_route(std::shared_ptr<lgsomeip::osabstraction::Address> address) {
    LGSOMEIP_LOG_DEBUG << "PacketRouterHost::remove_route / [Addr:" << address->to_string() << "]";

    std::lock_guard<std::recursive_mutex> guard(route_management_mutex_);

    for (auto service_it = registered_service_info_.begin(); service_it != registered_service_info_.end();) {
        auto& instance_map = service_it->second;
        for (auto instance_it = instance_map.begin(); instance_it != instance_map.end();) {
            auto& service_info = instance_it->second;
            std::shared_ptr<lgsomeip::osabstraction::Address> target_addr;
            if (service_info.tcpendpoint != nullptr && service_info.app_id <= 0) {
                target_addr = service_info.tcpendpoint->get_socket()->get_dst_address();

                if (target_addr != nullptr && *target_addr == *address) {
                    remove_connected_endpoint(service_info.tcpendpoint);
                }

                LGSOMEIP_LOG_DEBUG << "PacketRouterHost::remove_route / remove route info "
                                   << format_service_instance_id(service_it->first, instance_it->first);

                instance_it = instance_map.erase(instance_it);
            } else {
                ++instance_it;
            }
        }

        if (instance_map.empty()) {
            service_it = registered_service_info_.erase(service_it);
        } else {
            ++service_it;
        }
    }
}

// -----------------------------------------------------------------------------
//  PacketRouter Host : Event Subscribe Control Operation
// -----------------------------------------------------------------------------
std::int32_t PacketRouterHost::find_connection(std::uint16_t service_id, std::uint16_t instance_id,
                                               std::shared_ptr<lgsomeip::osabstraction::Address> address) {
    std::int32_t retfd = -1;
    struct RoutingServiceInfo* serviceinfo = nullptr;

    std::lock_guard<std::recursive_mutex> guard(route_management_mutex_);

    auto it = registered_service_info_.find(service_id);
    if (it == registered_service_info_.end())
        return retfd;

    if (it != registered_service_info_.end()) {
        auto& info = it->second;
        auto it2 = info.find(instance_id);
        if (it2 != info.end()) {
            serviceinfo = &(it2->second);
        }
    }

    if (serviceinfo == nullptr) {
        LGSOMEIP_LOG_INFO << "PacketRouterHost::find_connection / Service does not exist!";
    } else if (address->get_reliable() == false) {
        if (!address->is_multicast()) {
            LGSOMEIP_LOG_WARN << "PacketRouterHost::find_connection / There is no connection for UDP.";
            retfd = 0;
        } else {
            for (auto& connection : connected_endpoints_) {
                auto addr = connection.second.endpoint->get_socket()->get_src_address();
                if (addr == nullptr)
                    continue;
                if (addr->get_reliable() == true || addr->is_multicast() == false)
                    continue;
                if (addr->get_ip_address() == address->get_ip_address() &&
                    addr->get_port_address() == address->get_port_address()) {
                    retfd = connection.first;
                    break;
                }
            }
        }
    } else if (address->get_reliable() == true && serviceinfo->tcpendpoint == nullptr) {
        LGSOMEIP_LOG_WARN << "PacketRouterHost::find_connection / There is no TCP Connection for this service";
    } else {
        auto tcpendpoint = std::dynamic_pointer_cast<EndpointTCPServer<PacketRouterHost>>(serviceinfo->tcpendpoint);
        auto& clientfdlist = tcpendpoint->get_client_list();
        for (std::int32_t fd : clientfdlist) {
            auto clientaddr = connected_endpoints_[fd].endpoint->get_socket()->get_dst_address();
            if (clientaddr->get_ip_address() == address->get_ip_address() &&
                clientaddr->get_port_address() == address->get_port_address()) {
                retfd = fd;
                break;
            }
        }

        LGSOMEIP_LOG_DEBUG << "PacketRouterHost::find_connection / Addr = " << address->to_string()
                           << ", fd = " << retfd;
    }

    return retfd;
}

void PacketRouterHost::add_subscribe_route(std::uint16_t service_id, std::uint16_t instance_id, std::uint16_t event_id,
                                           std::uint16_t app_id,
                                           std::shared_ptr<lgsomeip::osabstraction::Address> address,
                                           std::uint16_t event_group_id) {
    if (app_id > 0) {
        LGSOMEIP_LOG_DEBUG << "PacketRouterHost::add_subscribe_route "
                           << format_service_instance_event_id(service_id, instance_id, event_id) << " add "
                           << format_named_id("AppID", app_id, 4)
                           << (address != nullptr ? "/" + address->to_string() : std::string());
    } else if (address != nullptr) {
        LGSOMEIP_LOG_DEBUG << "PacketRouterHost::add_subscribe_route "
                           << format_service_instance_event_id(service_id, instance_id, event_id) << " add Addr["
                           << address->to_string() << "]";
    } else {
        LGSOMEIP_LOG_DEBUG << "PacketRouterHost::add_subscribe_route "
                           << format_service_instance_event_id(service_id, instance_id, event_id);
    }

    std::lock_guard<std::recursive_mutex> guard(route_management_mutex_);

    auto& service_info = registered_service_info_[service_id][instance_id];
    auto& subscribe_list = service_info.eventinfos[event_id];

    if (app_id != 0) {
        // Enable Multicast Port
        if (address != nullptr && address->is_multicast() == true && service_info.multicasts.size() == 0) {
            std::int32_t fd_id = find_connection(service_id, instance_id, address);
            if (fd_id != -1) {
                service_info.multicasts.push_back(connected_endpoints_[fd_id].endpoint);
                LGSOMEIP_LOG_INFO << "PacketRouterHost::add_subscribe_route / reuse Connection, FD = " << fd_id;
            } else {
                // TODO: Handling DTLS in case of multicast, if necessary.
                // I'm not sure if DTLS can be applied to multicast.
                auto new_socket = std::make_shared<lgsomeip::osabstraction::UDPSocket>(address);
                new_socket->join_multicast(address->get_ip_address().c_str());
                new_socket->bind();
                std::shared_ptr<Endpoint> ep = std::make_shared<EndpointUDP<PacketRouterHost>>(this);
                ep->set_socket(new_socket);

                if (ep) {
                    ep->start_listen(get_multiplexer());
                    auto& info = connected_endpoints_[ep->get_socket()->get_socket_fd()];
                    info.endpoint = ep;
                    info.is_internal = false;
                    info.app_id = app_id;
                    info.endpoint->increase_reference_count();
                    service_info.multicasts.push_back(ep);
                }

                set_instance_id(service_id, instance_id, address);
                LGSOMEIP_LOG_INFO << "PacketRouterHost::add_subscribe_route / add Connection, FD = "
                                  << new_socket->get_socket_fd();
            }
        }

        // Internal Subscribe
        auto subscribe = std::find_if(std::begin(subscribe_list), std::end(subscribe_list),
                                      [&app_id](struct RoutingSubscribeInfo& i) { return i.app_id == app_id; });

        if (subscribe == std::end(subscribe_list)) {
            struct RoutingSubscribeInfo item;
            item.app_id = app_id;
            item.eventgroupid = event_group_id;
            item.endpoint = local_applications_[app_id].sender;
            subscribe_list.push_back(item);
        }
    } else {
        // External Subscribe
        if (address == nullptr) {
            LGSOMEIP_LOG_WARN << "PacketRouterHost::add_subscribe_route / IP Address is null "
                              << format_service_instance_event_id(service_id, instance_id, event_id);

            return;
        }

        if (address->get_reliable() == true) {
            auto subscribe = std::find_if(std::begin(subscribe_list), std::end(subscribe_list),
                                          [&address](struct RoutingSubscribeInfo& i) {
                                              auto cmpaddr = i.endpoint->get_socket()->get_dst_address();
                                              return cmpaddr != nullptr && *address == *cmpaddr;
                                          });

            if (subscribe == std::end(subscribe_list)) {
                int client_fd = find_connection(service_id, instance_id, address);
                if (client_fd > 0) {
                    struct RoutingSubscribeInfo item;
                    item.eventgroupid = event_group_id;
                    item.endpoint = connected_endpoints_[client_fd].endpoint;
                    item.destAddr = nullptr;
                    subscribe_list.push_back(item);

                    LGSOMEIP_LOG_DEBUG << "PacketRouterHost::add_subscribe_route "
                                       << format_service_instance_event_id(service_id, instance_id, event_id)
                                       << " is added for TCP";
                } else {
                    LGSOMEIP_LOG_WARN << "PacketRouterHost::add_subscribe_route / TCP Connection Error "
                                      << format_service_instance_event_id(service_id, instance_id, event_id)
                                      << ", addr: " << address->to_string();
                }
            } else {
                LGSOMEIP_LOG_DEBUG << "PacketRouterHost::add_subscribe_route "
                                   << format_service_instance_event_id(service_id, instance_id, event_id)
                                   << " already exists for TCP";
            }
        } else {
            auto subscribe = std::find_if(std::begin(subscribe_list), std::end(subscribe_list),
                                          [&address](struct RoutingSubscribeInfo& i) {
                                              return i.destAddr != nullptr && *i.destAddr == *address;
                                          });

            if (subscribe == std::end(subscribe_list)) {
                struct RoutingSubscribeInfo item;
                item.eventgroupid = event_group_id;
                item.endpoint = service_info.udpendpoint;
                item.destAddr = address;
                subscribe_list.push_back(item);

                LGSOMEIP_LOG_DEBUG << "PacketRouterHost::add_subscribe_route "
                                   << format_service_instance_event_id(service_id, instance_id, event_id)
                                   << " is added for UDP";
            } else {
                LGSOMEIP_LOG_DEBUG << "PacketRouterHost::add_subscribe_route "
                                   << format_service_instance_event_id(service_id, instance_id, event_id)
                                   << " already exists for UDP";
            }
        }
    }
}

void PacketRouterHost::remove_subscribe_route(std::uint16_t service_id, std::uint16_t instance_id,
                                              std::uint16_t event_id, std::uint16_t app_id,
                                              std::shared_ptr<lgsomeip::osabstraction::Address> address) {
    LGSOMEIP_LOG_DEBUG << "PacketRouterHost::remove_subscribe_route "
                       << format_service_instance_event_id(service_id, instance_id, event_id);

    if (app_id > 0) {
        LGSOMEIP_LOG_DEBUG << "=> remove " << format_named_id("AppID", app_id, 4);
    } else if (address != nullptr) {
        LGSOMEIP_LOG_DEBUG << "=> remove Addr[" << address->to_string() << "]";
    }

    std::lock_guard<std::recursive_mutex> guard(route_management_mutex_);

    auto service_it = registered_service_info_.find(service_id);
    if (service_it == registered_service_info_.end()) {
        LGSOMEIP_LOG_INFO << "PacketRouterHost::remove_subscribe_route / ServiceID not found "
                          << format_service_instance_event_id(service_id, instance_id, event_id);
        return;
    }

    auto instance_it = (service_it->second).find(instance_id);
    if (instance_it == (service_it->second).end()) {
        LGSOMEIP_LOG_INFO << "PacketRouterHost::remove_subscribe_route / InstanceID not found "
                          << format_service_instance_event_id(service_id, instance_id, event_id);
        return;
    }

    auto& service_info = instance_it->second;
    auto& event_map = service_info.eventinfos;
    auto event_it = event_map.find(event_id);
    if (event_it == event_map.end()) {
        LGSOMEIP_LOG_INFO << "PacketRouterHost::remove_subscribe_route / EventID not found "
                          << format_service_instance_event_id(service_id, instance_id, event_id);
        return;
    }

    auto& subscribe_list = event_it->second;
    if (subscribe_list.empty()) {
        LGSOMEIP_LOG_INFO << "PacketRouterHost::remove_subscribe_route / No subscription on EventID "
                          << format_service_instance_event_id(service_id, instance_id, event_id);
        event_map.erase(event_id);
        return;
    }

    if (app_id != 0) {
        auto item = std::find_if(std::begin(subscribe_list), std::end(subscribe_list),
                                 [&app_id](struct RoutingSubscribeInfo& i) { return i.app_id == app_id; });

        if (item != std::end(subscribe_list)) {
            subscribe_list.erase(item);
        }

        if (service_info.app_id == 0 && subscribe_list.empty()) {
            service_info.multicasts.clear();
        }
    } else {
        // TODO : remove external subscribe
        if (address == nullptr) {
            return;
        }

        auto item =
            std::find_if(std::begin(subscribe_list), std::end(subscribe_list), [&](struct RoutingSubscribeInfo& info) {
                if (info.endpoint->get_socket() == nullptr)
                    return false;

                auto sub_addr = info.endpoint->get_socket()->get_src_address();
                if (sub_addr == nullptr) {
                    sub_addr = info.endpoint->get_socket()->get_dst_address();
                }
                if (sub_addr->get_type() == AF_UNIX)
                    return false;
                auto cmp_addr = (sub_addr->get_reliable() == true) ? sub_addr : info.destAddr;
                if (cmp_addr != nullptr) {
                    return *cmp_addr == *address;
                }
                return false;
            });

        if (item != std::end(subscribe_list)) {
            subscribe_list.erase(item);
        }
    }

    if (subscribe_list.empty()) {
        LGSOMEIP_LOG_DEBUG << "PacketRouterHost::remove_subscribe_route / No more subscription on "
                           << format_service_instance_event_id(service_id, instance_id, event_id)
                           << ". Remove its event Info.";
        event_map.erase(event_id);
    }
}

void PacketRouterHost::remove_subscribe_route(std::uint16_t service_id, std::uint16_t instance_id,
                                              std::uint16_t event_id, std::uint16_t app_id) {
    remove_subscribe_route(service_id, instance_id, event_id, app_id, nullptr);
}

void PacketRouterHost::remove_subscribe_route(std::uint16_t app_id) {
    LGSOMEIP_LOG_DEBUG << "PacketRouterHost::remove_subscribe_route / " << format_named_id("AppID", app_id, 4);

    // Remove service info related to the app-id
    std::lock_guard<std::recursive_mutex> guard(route_management_mutex_);

    for (auto& service : registered_service_info_) {
        auto& instance_map = service.second;
        for (auto& instance : instance_map) {
            auto& service_info = instance.second;
            auto& event_map = service_info.eventinfos;

            auto event_it = event_map.begin();
            while (event_it != event_map.end()) {
                // remove subscribe info
                auto& subscribe_list = event_it->second;
                auto remove_item =
                    std::find_if(subscribe_list.begin(), subscribe_list.end(),
                                 [&app_id](struct RoutingSubscribeInfo& i) { return i.app_id == app_id; });

                if (remove_item != subscribe_list.end()) {
                    subscribe_list.erase(remove_item);
                }

                if (subscribe_list.empty()) {
                    if (service_info.app_id == 0) {
                        service_info.multicasts.clear();
                    }
                    LGSOMEIP_LOG_DEBUG << "PacketRouterHost::remove_subscribe_route / remove event subscription info "
                                       << format_service_instance_event_id(service.first, instance.first,
                                                                           event_it->first);
                    event_it = event_map.erase(event_it);
                } else {
                    ++event_it;
                }
            }
        }
    }
}

void PacketRouterHost::remove_subscribe_route(std::shared_ptr<lgsomeip::osabstraction::Address> address) {
    LGSOMEIP_LOG_DEBUG << "PacketRouterHost::remove_subscribe_route / [Addr:" << address->to_string() << "]";

    // Remove service info related to the addr
    std::lock_guard<std::recursive_mutex> guard(route_management_mutex_);

    for (auto& service : registered_service_info_) {
        auto& instance_map = service.second;
        for (auto& instance : instance_map) {
            auto& service_info = instance.second;
            auto& event_map = service_info.eventinfos;

            auto event_it = event_map.begin();
            while (event_it != event_map.end()) {
                // remove subscribe info
                auto& subscribe_list = event_it->second;
                auto remove_item =
                    std::find_if(subscribe_list.begin(), subscribe_list.end(), [&](struct RoutingSubscribeInfo& info) {
                        if (info.endpoint->get_socket() == nullptr)
                            return false;

                        auto sub_addr = info.endpoint->get_socket()->get_src_address();
                        if (sub_addr == nullptr) {
                            sub_addr = info.endpoint->get_socket()->get_dst_address();
                        }

                        if (sub_addr != nullptr) {
                            if (sub_addr->get_type() == AF_UNIX)
                                return false;

                            auto cmp_addr = (sub_addr->get_reliable() == true) ? sub_addr : info.destAddr;
                            if (cmp_addr != nullptr) {
                                return *cmp_addr == *address;
                            }
                        }
                        return false;
                    });

                if (remove_item != subscribe_list.end()) {
                    subscribe_list.erase(remove_item);
                }

                if (subscribe_list.empty()) {
                    LGSOMEIP_LOG_DEBUG << "PacketRouterHost::remove_subscribe_route / remove event subscription info "
                                       << format_service_instance_event_id(service.first, instance.first,
                                                                           event_it->first);
                    event_it = event_map.erase(event_it);
                } else {
                    ++event_it;
                }
            }
        }
    }
}

// -----------------------------------------------------------------------------
//  PacketRouter Host : Application Control Operation
// -----------------------------------------------------------------------------
void PacketRouterHost::on_application_control_message(std::shared_ptr<Endpoint> endpoint,
                                                      std::shared_ptr<MessageSD> message) {
    if (message == nullptr || message->options().empty()) {
        LGSOMEIP_LOG_WARN << "PacketRouterHost::on_application_control_message / Missing application option";
        return;
    }

    std::uint16_t fd = endpoint->get_socket()->get_socket_fd();

    LGSOMEIP_LOG_DEBUG << "PacketRouterHost::on_application_control_message / fd: " << fd
                       << ", AppName: " << message->option(0).get_configuration(SOMEIP_APPLICATION_NAME) << ", "
                       << format_named_id("AppID", message->get_request_id() >> 16, 4);

    std::lock_guard<std::recursive_mutex> guard(route_management_mutex_);

    auto it = connected_endpoints_.find(fd);
    if (it != connected_endpoints_.end()) {
        std::string appname = message->option(0).get_configuration(SOMEIP_APPLICATION_NAME);
        std::uint16_t app_id = message->get_request_id() >> 16;
        if (!is_valid_application_name(appname) || app_id == 0) {
            LGSOMEIP_LOG_ERROR << "PacketRouterHost::on_application_control_message / Invalid application identity";
            return;
        }

        auto existing_application = local_applications_.find(app_id);
        if (existing_application != local_applications_.end() && existing_application->second.receiver != endpoint) {
            LGSOMEIP_LOG_ERROR
                << "PacketRouterHost::on_application_control_message / Application ID is already registered";
            return;
        }

        struct RoutingApplicationInfo connector;
        std::shared_ptr<lgsomeip::osabstraction::Socket> send_socket =
            EndpointUtils::create_local_socket<lgsomeip::osabstraction::TCPClientSocket>(app_id, true);

        if (send_socket != nullptr) {
            LGSOMEIP_LOG_INFO << "PacketRouterHost::on_application_control_message / fd: " << fd
                              << ", AppName: " << appname << ", " << format_named_id("AppID", app_id, 4)
                              << ", path: " << send_socket->get_dst_address()->get_file_path();

            connector.name = appname;
            connector.sender = std::make_shared<Endpoint>();
            connector.sender->set_socket(send_socket);
            connector.receiver = endpoint;
            local_applications_[app_id] = connector;

            (it->second).app_id = app_id;
        } else {
            LGSOMEIP_LOG_ERROR << "PacketRouterHost::on_application_control_message / Connection Error. fd: " << fd
                               << ", " << format_named_id("AppID", app_id, 4);
        }
    } else {
        LGSOMEIP_LOG_ERROR << "PacketRouterHost::on_application_control_message / Connection is not found. fd: " << fd
                           << ", " << format_named_id("AppID", message->get_request_id() >> 16, 4);
    }
}

void PacketRouterHost::send_internal_message(std::uint8_t* message, std::size_t message_length,
                                             std::uint16_t target_app_id) {
    auto application = local_applications_.find(target_app_id);
    if (application != local_applications_.end() && application->second.sender != nullptr) {
        application->second.sender->send_message(message, message_length);
    } else {
        LGSOMEIP_LOG_WARN << "PacketRouterHost::send_internal_message / No Receiver for "
                          << format_named_id("AppID", target_app_id, 4);
    }
}

void PacketRouterHost::send_external_sd_message(std::uint8_t* message, std::size_t message_length,
                                                std::shared_ptr<lgsomeip::osabstraction::Address> target_address,
                                                bool multicast) {
    if (service_discovery_unicast_ != nullptr) {
        if (target_address != nullptr) {
            service_discovery_unicast_->send_message(message, message_length, target_address);
        } else {
            service_discovery_unicast_->send_message(message, message_length, service_discovery_address_);
        }
    } else {
        LGSOMEIP_LOG_INFO << "PacketRouterHost::send_external_sd_message / Unicast socket for SOME/IP-SD is not ready!";
    }
}

std::shared_ptr<lgsomeip::osabstraction::Address> PacketRouterHost::make_address(bool reliable, std::uint16_t port,
                                                                                 std::uint8_t vlan_priority) {
    std::shared_ptr<lgsomeip::osabstraction::Address> addr = make_address();
    addr->set_reliable(reliable);
    addr->set_port_address(port);
    addr->set_vlan_priority(vlan_priority);
    return addr;
}

std::shared_ptr<lgsomeip::osabstraction::Address> PacketRouterHost::make_address() {
    std::shared_ptr<lgsomeip::osabstraction::Address> addr = nullptr;
    if (host_->get_configuration()->get_ip_type() == 6) {
        addr = std::make_shared<lgsomeip::osabstraction::IP6Address>();
    } else {
        addr = std::make_shared<lgsomeip::osabstraction::IP4Address>();
    }

    return addr;
}

void PacketRouterHost::check_and_send_magic_cookies() {
    std::lock_guard<std::recursive_mutex> guard(route_management_mutex_);

    for (auto& element : connected_endpoints_) {
        auto& routing_info = element.second;

        // Ignore checking in case there is no endpoint for this element.
        if (routing_info.endpoint == nullptr) {
            continue;
        }

        // Check if magic cookie is enabled, connection is TCP, and is NOT internal connection.
        // serverfd == 0 means that this endpoint is EndpointTCPServer.
        // There is nothing to do with this listening only endpoint,
        // so move on to the next enpoint.
        if ((routing_info.endpoint->get_magic_cookie_enabled() == true) &&
            (routing_info.endpoint->get_socket()->get_reliable() == true) && (routing_info.is_internal == false) &&
            (routing_info.serverfd != 0)) {
            // Check if timer is expired or do nothing.
            auto endpoint = std::static_pointer_cast<EndpointTCPClient<PacketRouterHost>>(routing_info.endpoint);
            std::chrono::system_clock::time_point last_cookie_sent = endpoint->get_last_cookie_sent_time();
            std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

            if (now - last_cookie_sent >= std::chrono::milliseconds(kMagicCookiePeriodMs)) {
                bool is_service_provider = false;
                if (routing_info.serverfd > 0) {
                    // When we are service provider, EndpointTCPServer is created,
                    // and its FD is saved when new consumer connects to us and corresponding
                    // new EndpointTCPClient is created. See EndpointTCPServer::on_connect().
                    is_service_provider = true;
                }

                LGSOMEIP_LOG_DEBUG << "PacketRouterHost::check_and_send_magic_cookies / Send Magic cookie("
                                   << (is_service_provider ? "Provider)" : "Consumer)") << "to Addr["
                                   << endpoint->get_socket()->get_dst_address()->get_ip_address() << "]";
                endpoint->send_magic_cookie(is_service_provider);
                endpoint->set_last_cookie_sent_time(now);
            }
        }
    }
}

void PacketRouterHost::register_connected_endpoint(std::shared_ptr<Endpoint> ep, std::uint16_t app_id) {
    if (ep->get_reference_count() == 0) {
#if defined(ENABLE_TLS)
        if (ep->get_socket()->is_secure_connection()) {
            ep->start_listen(get_secure_multiplexer());
        } else {
            ep->start_listen(get_multiplexer());
        }
#else  // ENABLE_TLS
        ep->start_listen(get_multiplexer());
#endif // ENABLE_TLS
        RoutingConnectionInfo new_conn_info;
        new_conn_info.endpoint = ep;
        new_conn_info.app_id = app_id;
        if (app_id == 0) {
            new_conn_info.serverfd = -1;
        }
        std::lock_guard<std::recursive_mutex> guard(route_management_mutex_);

        connected_endpoints_[ep->get_socket()->get_socket_fd()] = new_conn_info;
    }
    ep->increase_reference_count();
}

void PacketRouterHost::remove_app_before_connection(const std::int32_t& file_descriptor,
                                                    const RoutingConnectionInfo& connection_info) {
    LGSOMEIP_LOG_INFO << "PacketRouterHost::remove_app_before_connection / Disconnected at initial state!";

    connection_info.endpoint->decrease_reference_count();

    LGSOMEIP_LOG_DEBUG << "PacketRouterHost::remove_app_before_connection / referenceCount of this endpoint = "
                       << static_cast<std::uint16_t>(connection_info.endpoint->get_reference_count())
                       << ", ApplicationControlMessage has not been received yet";

    if (connection_info.endpoint->get_reference_count() == 0) {
        std::lock_guard<std::recursive_mutex> guard(route_management_mutex_);

        connected_endpoints_.erase(file_descriptor);
    }
    local_receiver_->remove_client_endpoint(file_descriptor);
}

void PacketRouterHost::remove_connected_endpoint(std::shared_ptr<Endpoint> endpoint, bool multicast) {
    if (endpoint != nullptr) {
        std::int32_t fd = endpoint->get_socket()->get_socket_fd();
        bool is_reliable = endpoint->get_socket()->get_reliable();
        LGSOMEIP_LOG_DEBUG << "PacketRouterHost::remove_connected_endpoint / "
                           << (multicast ? "Multicast" : (is_reliable ? "TCP" : "UDP")) << " [FD:" << fd << "]";

        std::lock_guard<std::recursive_mutex> guard(route_management_mutex_);

        auto conn_info_it = connected_endpoints_.find(fd);
        if (conn_info_it != connected_endpoints_.end()) {
            auto& connection_info = conn_info_it->second;
            connection_info.endpoint->decrease_reference_count();

            LGSOMEIP_LOG_DEBUG << "PacketRouterHost::remove_connected_endpoint / referenceCount of this endpoint = "
                               << static_cast<std::uint16_t>(connection_info.endpoint->get_reference_count())
                               << ", Port = " << endpoint->get_socket()->get_src_address()->get_port_address();

            if (connection_info.endpoint->get_reference_count() == 0) {
                connected_endpoints_.erase(fd);
            }
        }
    }
}

#if defined(ENABLE_QNX_MESSAGE_PASSING)
#if !defined(ENABLE_SOMEIP_IPC)
void PacketRouterHost::on_message_passing_connect(std::shared_ptr<Endpoint> server_endpoint,
                                                  std::shared_ptr<Endpoint> client_endpoint)
#else
void PacketRouterHost::on_message_passing_connect(std::shared_ptr<Endpoint> server_endpoint,
                                                  std::shared_ptr<Endpoint> client_endpoint, int connection_id)
#endif // ENABLE_SOMEIP_IPC
{
    std::lock_guard<std::mutex> lock(mutex_processing_);

    client_endpoint->set_app_id(0);
    client_endpoint->increase_reference_count();
    client_endpoint->start_listen();
}

#if !defined(ENABLE_SOMEIP_IPC)
void PacketRouterHost::on_message_passing_disconnect(std::shared_ptr<Endpoint> endpoint)
#else
void PacketRouterHost::on_message_passing_disconnect(std::shared_ptr<Endpoint> endpoint, int connection_id)
#endif // ENABLE_SOMEIP_IPC
{
    std::lock_guard<std::mutex> lock(mutex_processing_);

    std::uint16_t appId = endpoint->get_app_id();
    LGSOMEIP_LOG_INFO << "PacketRouterHost::onMessagePassingDisconnect / Listener Stop "
                      << format_named_id("AppID", appId, 4);

    if (appId == 0) {
        endpoint->decrease_reference_count();
        return;
    }

    // Remove service info related to the appId
    remove_subscribe_route(appId);

    // Remove route info for all provided/consumed services related with appId.
    remove_route(appId);

    // Remove request map for routing request/response.
    auto reqMapIt = request_map_.begin();
    while (reqMapIt != request_map_.end()) {
        if (reqMapIt->second.endpoint == local_applications_[appId].sender) {
            reqMapIt = request_map_.erase(reqMapIt);
        } else {
            ++reqMapIt;
        }
    }

    // Stop Application Socket
    local_applications_[appId].receiver->stop_listen();
    local_applications_.erase(appId);

    endpoint->decrease_reference_count();

    LGSOMEIP_LOG_DEBUG << "PacketRouterHost::onMessagePassingDisconnect / referenceCount of this endpoint = "
                       << endpoint->get_reference_count() << ", " << format_named_id("AppID", appId, 4);

    host_->on_disconnected_application(appId);
}

void PacketRouterHost::on_message_passing_message(std::shared_ptr<Endpoint> endpoint, std::uint8_t* message_data,
                                                  std::size_t message_length) {
    std::lock_guard<std::mutex> lock(mutex_processing_);

    bool isSDType = MessageBuilder::is_sd_message(message_data, message_length);

    print_byte_message("PacketRouterHost::on_message_passing_message", message_data, message_length);

    if (isSDType) {
        // Check if this message is for trigger to create the files including the current states of SOME/IP services
        if (message_length >= 26 && message_data[24] == 0xF1 && message_data[25] == 0xF2) {
#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
            SomeipPacketStatistics::get_instance().increase_sd_type_packet(
                SomeipPacketStatistics::kIncoming, 0, SomeipPacketStatistics::kSomeipSdTriggerDumpSomeipServices);
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS

            if (message_data[12] != 0 || message_data[13] != 0 || message_data[14] != 2 || message_data[15] != 0) {
                LGSOMEIP_LOG_WARN << "PacketRouterHost::on_message / Invalid trigger message";

                return;
            }

            // Create the files including the current states of SOME/IP services
            process_trigger_writing_service_state();

            return;
        }

        auto sd_message = MessageBuilder::create<SOMEIPSD>();
        if (false == MessageBuilder::build_message(*sd_message, message_data, message_length)) {
            LGSOMEIP_LOG_INFO << "PacketRouterHost::onMessagePassingMessage / Fail to build_message of the SOMEIPSD!!!";
            return;
        }
        if (sd_message->entries().empty()) {
            LGSOMEIP_LOG_WARN << "PacketRouterHost::on_message_passing_message / SD message has no entries";
            return;
        }

        if (sd_message->entry(0).get_type() == SOMEIP_SD_ENTRY::INTERNAL::TYPEID) {
            // Process Applcation Control Message
            on_message_passing_application_control_message(endpoint, sd_message);
        } else {
            // Process Service Control Message : send to service manager
            host_->on_internal_message(sd_message);
        }
    } else {
#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
        std::uint32_t incoming_message_id;
        get_byte_stream(&incoming_message_id, message);
        SomeipPacketStatistics::get_instance().increase_received_packet(SomeipPacketStatistics::kOutgoing,
                                                                        incoming_message_id);
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS

        on_internal_message(endpoint, message_data, message_length);
    }
}

void PacketRouterHost::on_message_passing_application_control_message(std::shared_ptr<Endpoint> endpoint,
                                                                      std::shared_ptr<MessageSD> message) {
    if (message == nullptr || message->options().empty()) {
        LGSOMEIP_LOG_WARN
            << "PacketRouterHost::on_message_passing_application_control_message / Missing application option";
        return;
    }

    std::string appname = message->option(0).get_configuration(SOMEIP_APPLICATION_NAME);
    std::uint16_t app_id = message->get_request_id() >> 16;

    if (!is_valid_application_name(appname) || app_id == 0) {
        LGSOMEIP_LOG_ERROR
            << "PacketRouterHost::on_message_passing_application_control_message / Invalid application identity";
        return;
    }
    auto existing_application = local_applications_.find(app_id);
    if (existing_application != local_applications_.end() && existing_application->second.receiver != endpoint) {
        LGSOMEIP_LOG_ERROR << "PacketRouterHost::on_message_passing_application_control_message / Application ID is "
                              "already registered";
        return;
    }

    struct RoutingApplicationInfo connector;

    LGSOMEIP_LOG_INFO << "PacketRouterHost::onMessagePassingApplicationControlMessage / AppName: " << appname << ", "
                      << format_named_id("AppID", app_id, 4);

    connector.name = appname;
    connector.sender =
        std::make_shared<EndpointMessagePassingSender>(EndpointUtils::create_message_passing_channel_name(app_id));
    connector.receiver = endpoint;
    local_applications_[app_id] = connector;

    endpoint->set_app_id(app_id);
}

void PacketRouterHost::message_passing_set_timer(const int32_t id, const std::uint32_t interval_milliseconds,
                                                 const bool periodic, std::function<void(void)> handler) {
    if (local_message_passing_receiver_ != nullptr) {
        auto itor = timer_handlers_.find(id);
        if (itor == timer_handlers_.end()) {
            local_message_passing_receiver_->set_timer(id, interval_milliseconds, periodic);

            timer_handlers_[id] = handler;
        }
    }
}

void PacketRouterHost::message_passing_kill_timer(const int32_t id) {
    if (local_message_passing_receiver_ != nullptr) {
        auto itor = timer_handlers_.find(id);
        if (itor != timer_handlers_.end()) {
            local_message_passing_receiver_->kill_timer(id);

            timer_handlers_.erase(itor);
        }
    }
}

void PacketRouterHost::on_message_passing_timer(const int32_t id) {
    std::lock_guard<std::mutex> lock(mutex_processing_);

    auto itor = timer_handlers_.find(id);
    if (itor != timer_handlers_.end()) {
        if (itor->second != nullptr) {
            itor->second();
        }
    }
}

#endif // ENABLE_QNX_MESSAGE_PASSING

} // namespace lgsomeip
