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
#include <sstream>
#include <vector>

#include <endpoint/EndpointUtils.h>
#include <exception/ApplicationError.h>
#include <message/Message.h>
#include <packetrouter/PacketRouterProxy.h>
#include <socket/LocalAddress.h>
#include <utils/log/formatLog.h>
#include <utils/log/logger.h>

#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
#include <utils/statistics/SomeipPacketStatistics.h>
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS

#define LGSOMEIP_RETRY_CONNECT_TIMER_ID 1
#define LGSOMEIP_RETRY_CONNECT_TIME 1000

namespace lgsomeip {

// PacketRouterProxy public methods.
PacketRouterProxy::PacketRouterProxy(ApplicationManager* host)
    : host_(host), multiplexer_(nullptr), receiver_(nullptr), listener_(nullptr), sender_(nullptr),
      message_passing_receiver_(nullptr), message_passing_listener_(nullptr), message_passing_sender_(nullptr) {}

void PacketRouterProxy::init() {
#if defined(ENABLE_QNX_MESSAGE_PASSING)
    message_passing_receiver_ = std::make_shared<EndpointMessagePassingServer>(
        this, EndpointUtils::create_message_passing_channel_name(host_->get_application_id()));

    auto sender = std::make_shared<EndpointMessagePassingSender>(
        EndpointUtils::create_message_passing_channel_name(SOMEIP_DAEMON_ID));
    if (sender->is_connected()) {
        message_passing_sender_ = sender;
    } else {
        message_passing_sender_ = nullptr;
        start_retry_connect();
    }
#else
    multiplexer_ = std::make_shared<lgsomeip::osabstraction::Multiplexer>();
    multiplexer_->set_timeout(std::bind(&PacketRouterProxy::do_connect, this));

    std::shared_ptr<lgsomeip::osabstraction::TCPServerSocket> recv_socket =
        EndpointUtils::create_local_socket<lgsomeip::osabstraction::TCPServerSocket>(host_->get_application_id(), true);
    if (recv_socket != nullptr) {
        recv_socket->listen();
        receiver_ = std::make_shared<EndpointTCPServer<PacketRouterProxy>>(this);
        receiver_->set_socket(recv_socket);
    } else {
        LGSOMEIP_LOG_FATAL << "PacketRouterProxy::init / fail - local socket creating error";
        exit(-1);
    }

    std::shared_ptr<lgsomeip::osabstraction::Socket> send_socket =
        EndpointUtils::create_local_socket<lgsomeip::osabstraction::TCPClientSocket>(SOMEIP_DAEMON_ID, true);

    if (send_socket != nullptr) {
        LGSOMEIP_LOG_INFO << "PacketRouterProxy::init / sendSocket fd = " << send_socket->get_socket_fd();
        sender_ = std::make_shared<Endpoint>();
        sender_->set_socket(send_socket);
    } else {
        LGSOMEIP_LOG_WARN << "PacketRouterProxy::init / Connection Error";
        sender_ = nullptr;
    }
#endif // ENABLE_QNX_MESSAGE_PASSING
}

#if defined(ENABLE_SOMEIP_IPC)
bool PacketRouterProxy::find_ipc_connection_info_service_id(std::uint16_t service_id) {
    if (ipc_connection_info_.find(service_id) == ipc_connection_info_.end())
        return false;
    return true;
}
bool PacketRouterProxy::find_ipc_connection_info_instance_id(std::uint16_t service_id, std::uint16_t instance_id) {
    auto& ipc_connection_info_service = ipc_connection_info_[service_id];
    if (ipc_connection_info_service.find(instance_id) == ipc_connection_info_service.end())
        return false;
    return true;
}

bool PacketRouterProxy::find_ipc_connection_info_event_group_id(std::uint16_t service_id, std::uint16_t instance_id,
                                                                std::uint16_t event_group_id) {
    auto& ipc_connection_info_instance = ipc_connection_info_[service_id][instance_id];
    if (ipc_connection_info_instance.find(event_group_id) == ipc_connection_info_instance.end())
        return false;
    return true;
}

bool PacketRouterProxy::find_ipc_connection_info_sender(std::uint16_t service_id, std::uint16_t instance_id,
                                                        std::uint16_t event_group_id, bool is_provider,
                                                        std::shared_ptr<Endpoint> ipc_sender) {
    auto& ipc_connection_info_event_group = ipc_connection_info_[service_id][instance_id][event_group_id];
    for (auto sender : ipc_connection_info_event_group) {
        if (sender.first == is_provider && sender.second == ipc_sender)
            return true;
    }

    return false;
}

void PacketRouterProxy::set_ipc_connection_info_service_id(std::uint16_t service_id, std::uint16_t instance_id,
                                                           std::uint16_t event_group_id, bool is_provider,
                                                           std::shared_ptr<Endpoint> ipc_sender) {
    std::vector<std::pair<bool, std::shared_ptr<Endpoint>>> ipc_endpoint_list;
    ipc_endpoint_list.push_back({is_provider, ipc_sender});

    std::map<std::uint16_t, std::vector<std::pair<bool, std::shared_ptr<Endpoint>>>> ipc_connection_event_group;
    ipc_connection_event_group.insert(std::make_pair(event_group_id, ipc_endpoint_list));

    std::map<std::uint16_t, std::map<std::uint16_t, std::vector<std::pair<bool, std::shared_ptr<Endpoint>>>>>
        ipc_connection_instance;
    ipc_connection_instance.insert({instance_id, ipc_connection_event_group});

    ipc_connection_info_.insert({service_id, ipc_connection_instance});
}

void PacketRouterProxy::set_ipc_connection_info_instance_id(std::uint16_t service_id, std::uint16_t instance_id,
                                                            std::uint16_t event_group_id, bool is_provider,
                                                            std::shared_ptr<Endpoint> ipc_sender) {
    std::vector<std::pair<bool, std::shared_ptr<Endpoint>>> ipc_endpoint_list;
    ipc_endpoint_list.push_back({is_provider, ipc_sender});

    std::map<std::uint16_t, std::vector<std::pair<bool, std::shared_ptr<Endpoint>>>> ipc_connection_event_group;
    ipc_connection_event_group.insert(std::make_pair(event_group_id, ipc_endpoint_list));

    ipc_connection_info_[service_id].insert({instance_id, ipc_connection_event_group});
}

void PacketRouterProxy::set_ipc_connection_info_event_group_id(std::uint16_t service_id, std::uint16_t instance_id,
                                                               std::uint16_t event_group_id, bool is_provider,
                                                               std::shared_ptr<Endpoint> ipc_sender) {
    std::vector<std::pair<bool, std::shared_ptr<Endpoint>>> ipc_endpoint_list;
    ipc_endpoint_list.push_back({is_provider, ipc_sender});
    ipc_connection_info_[service_id][instance_id].insert({event_group_id, ipc_endpoint_list});
}

void PacketRouterProxy::set_ipc_connection_info(std::uint16_t service_id, std::uint16_t instance_id,
                                                std::uint16_t event_group_id, bool is_provider,
                                                std::shared_ptr<Endpoint> ipc_sender) {
    LGSOMEIP_LOG_INFO << "PacketRouterProxy::setIpcConnectionInfo "
                      << format_service_instance_id(service_id, instance_id) << " "
                      << format_named_id("EventGroupID", event_group_id, 4)
                      << (is_provider ? " Provider" : " Consumer");

    if (find_ipc_connection_info_service_id(service_id)) {
        if (find_ipc_connection_info_instance_id(service_id, instance_id)) {
            if (find_ipc_connection_info_event_group_id(service_id, instance_id, event_group_id)) {
                if (find_ipc_connection_info_sender(service_id, instance_id, event_group_id, is_provider, ipc_sender)) {
                    LGSOMEIP_LOG_INFO << "PacketRouterProxy::setIpcConnectionInfo already have "
                                      << format_service_instance_id(service_id, instance_id) << " "
                                      << format_named_id("EventGroupID", event_group_id, 4)
                                      << (is_provider ? " Provider" : " Consumer");
                } else {
                    auto& ipc_info = ipc_connection_info_[service_id][instance_id][event_group_id];
                    bool ipc_set_flag = true;
                    if (ipc_info.size() > 0) {
                        for (auto info : ipc_info) {
                            if (info.first == is_provider && info.second == ipc_sender) {
                                ipc_set_flag = false;
                                break;
                            }
                        }
                        if (ipc_set_flag) {
                            LGSOMEIP_LOG_INFO << "PacketRouterProxy::setIpcConnectionInfo Update "
                                              << format_service_instance_id(service_id, instance_id) << " "
                                              << format_named_id("EventGroupID", event_group_id, 4)
                                              << (is_provider ? " Provider" : " Consumer");
                            ipc_info.push_back({is_provider, ipc_sender});
                        }
                    }
                }
            } else {
                LGSOMEIP_LOG_INFO << "PacketRouterProxy::setIpcConnectionInfo new add "
                                  << format_service_instance_id(service_id, instance_id) << " "
                                  << format_named_id("EventGroupID", event_group_id, 4);
                set_ipc_connection_info_event_group_id(service_id, instance_id, event_group_id, is_provider,
                                                       ipc_sender);
            }
        } else {
            LGSOMEIP_LOG_INFO << "PacketRouterProxy::setIpcConnectionInfo add new "
                              << format_service_instance_id(service_id, instance_id);
            set_ipc_connection_info_instance_id(service_id, instance_id, event_group_id, is_provider, ipc_sender);
        }
    } else {
        LGSOMEIP_LOG_INFO << "PacketRouterProxy::setIpcConnectionInfo for New List for "
                          << format_named_id("ServiceID", service_id, 4);
        set_ipc_connection_info_service_id(service_id, instance_id, event_group_id, is_provider, ipc_sender);
    }
}
void PacketRouterProxy::add_ipc_route(std::uint16_t service_id, std::uint16_t instance_id, std::uint16_t event_group_id,
                                      std::uint16_t app_id, bool is_provider) {
    std::lock_guard<std::mutex> lock(mutex_processing_);
    if (ipc_senders_.find(app_id) == ipc_senders_.end()) {
#if defined(ENABLE_QNX_MESSAGE_PASSING)
        std::shared_ptr<Endpoint> ipc_sender =
            std::make_shared<EndpointMessagePassingSender>(EndpointUtils::create_message_passing_channel_name(app_id));
        if (ipc_sender == nullptr) {
            LGSOMEIP_LOG_ERROR << "PacketRouterProxy::add_ipc_route / ipc_sender Connection Error";
            return;
        } else {
            LGSOMEIP_LOG_INFO << "PacketRouterProxy::addIpcRoute for QNX Message Passing by "
                              << format_named_id("AppID", app_id, 4);
        }
#else
        std::shared_ptr<lgsomeip::osabstraction::Socket> ipc_send_socket =
            EndpointUtils::create_local_socket<lgsomeip::osabstraction::TCPClientSocket>(app_id, true);

        if (ipc_send_socket == nullptr) {
            LGSOMEIP_LOG_ERROR << "PacketRouterProxy::add_ipc_route / ipc_send_socket Connection Error";
            return;
        }

        LGSOMEIP_LOG_INFO << "PacketRouterProxy::add_ipc_route / ipc_send_socket "
                          << format_named_id("AppID", app_id, 4) << ", fd = " << ipc_send_socket->get_socket_fd();
        std::shared_ptr<Endpoint> ipc_sender = std::make_shared<Endpoint>();
        ipc_sender->set_socket(ipc_send_socket);
#endif
        ipc_senders_.insert({app_id, ipc_sender});
    }
    set_ipc_connection_info(service_id, instance_id, event_group_id, is_provider, ipc_senders_[app_id]);
}
#endif // ENABLE_SOMEIP_IPC

void PacketRouterProxy::start() {
    LGSOMEIP_LOG_DEBUG << "PacketRouterProxy::start";

    if (running_ == false) {
        running_ = true;

        // Connect the receiver socket to the multiplexer.
        if (multiplexer_ != nullptr) {
            multiplexer_->start();
            if (receiver_ != nullptr) {
                receiver_->start_listen(get_multiplexer());
            }
        }

        if (message_passing_receiver_ != nullptr) {
            message_passing_receiver_->start_listen();
        }

        // Send an application-registration message to the daemon.
        if (sender_ != nullptr) {
            do_register_application();
        }

        if (message_passing_sender_ != nullptr) {
            do_register_application();
        }
    }
}

void PacketRouterProxy::stop() {
    if (running_ == true) {
        // Stop any pending connection retry.
        stop_retry_connect();

        // Send an application-unregistration message to the daemon.
        do_un_register_application();

        // Disconnect the receiver socket from the multiplexer.
        if (receiver_ != nullptr) {
            receiver_->stop_listen();
        }
        if (multiplexer_ != nullptr) {
            multiplexer_->stop();
        }

        if (message_passing_receiver_ != nullptr) {
            message_passing_receiver_->stop_listen();
        }

        running_ = false;
    }
}

void PacketRouterProxy::join() {
    std::mutex mutex;
    std::unique_lock<std::mutex> its_lock(mutex);
    do {
        if (multiplexer_ != nullptr) {
            multiplexer_->join();
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    } while (running_);
}

void PacketRouterProxy::on_connect(std::shared_ptr<Endpoint> server_endpoint,
                                   std::shared_ptr<Endpoint> client_endpoint) {
#if !defined(ENABLE_SOMEIP_IPC)
    if (receiver_ == server_endpoint) {
        listener_ = client_endpoint;
        listener_->start_listen(get_multiplexer());
    }
#else
    std::uint32_t serverfd = server_endpoint->get_socket()->get_socket_fd();
    std::uint32_t clientfd = client_endpoint->get_socket()->get_socket_fd();
    if (receiver_ == server_endpoint) {
        LGSOMEIP_LOG_INFO << "PacketRouterProxy::on_connect"
                          << " receiver_ : " << receiver_->get_socket()->get_socket_fd() << " clientfd : " << clientfd
                          << " serverfd : " << serverfd;

        if (connected_endpoints_.find(clientfd) != connected_endpoints_.end()) {
            struct RoutingConnectionInfo& info = connected_endpoints_[clientfd];
            info.endpoint->increase_reference_count();
        } else {
            struct RoutingConnectionInfo info;
            info.endpoint = client_endpoint;
            info.is_internal = true;
            info.app_id = 0;
            info.endpoint->increase_reference_count();
            connected_endpoints_[clientfd] = info;
            client_endpoint->start_listen(get_multiplexer());
        }
    } else {
        LGSOMEIP_LOG_ERROR << "PacketRouterProxy::on_connect() / Invalid Case";
    }
#endif // ENABLE_SOMEIP_IPC
}

void PacketRouterProxy::on_message(std::shared_ptr<Endpoint> endpoint, std::uint8_t* message_data,
                                   std::size_t message_length) {
    print_byte_message("PacketRouterProxy::on_message", message_data, message_length);

    bool is_sd_message = MessageBuilder::is_sd_message(message_data, message_length);

    if (is_sd_message) {
        std::shared_ptr<MessageSD> sdmessage = MessageBuilder::create<SOMEIPSD>();

        // Process the service-control message and send it to ApplicationManager.
        if (false == MessageBuilder::build_message(*sdmessage, message_data, message_length)) {
            LGSOMEIP_LOG_INFO << "PacketRouterProxy::on_message / Fail to build_message of the SOMEIPSD!!!";
            return;
        }
        host_->on_message(sdmessage);
    } else {
        std::shared_ptr<MessageSOMEIP> someip_message = MessageBuilder::create<SOMEIP>();
        if ((host_->get_configuration())->is_e2e_enabled()) {
            if (message_data == nullptr || message_length <= SOMEIP_HEADER::POS::RETURNCODE) {
                LGSOMEIP_LOG_INFO << "PacketRouterProxy::on_message / Ignore truncated SOME/IP message";
                return;
            }
            std::uint8_t return_code;
            get_byte_stream(&return_code, message_data + SOMEIP_HEADER::POS::RETURNCODE);
            if (return_code == 0x20) {
                someip_message->set_is_valid_crc(false);
            }
        }

        // Process the SOME/IP message and send it to ApplicationManager.
        if (false == MessageBuilder::build_message(*someip_message, message_data, message_length)) {
            LGSOMEIP_LOG_INFO << "PacketRouterProxy::on_message / Fail to build_message of the SOMEIP!!!";
            return;
        }

#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
        SomeipPacketStatistics::get_instance().increase_received_packet(SomeipPacketStatistics::kIncoming,
                                                                        someip_message->get_message_id());
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS

        someip_message->set_instance_id(endpoint->get_instance_id());
        try {
            host_->on_message(someip_message);
        } catch (const ApplicationErrorException& e) { // when app is shut down after offering.
            LGSOMEIP_LOG_INFO << "PacketRouterProxy::on_message() got " << e.what() << ", "
                              << format_named_id("MessageID", someip_message->get_message_id(), 8) << ", "
                              << format_named_id("RequestID", someip_message->get_request_id(), 8) << ", "
                              << format_named_id("MessageType", someip_message->get_message_type(), 2);

            auto response = MessageBuilder::create_response_message(*someip_message);
            if (response != nullptr) {
                response->set_message_type(SOMEIP_MESSAGE_TYPE::ERROR);
                response->set_return_code(e.get_error_code());
                response->set_payload(nullptr, 0);
                send_message(response);
            }

#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
            SomeipPacketStatistics::get_instance().increase_dropped_packet(
                SomeipPacketStatistics::kIncoming, someip_message->get_message_id(),
                SomeipPacketStatistics::kApplicationManagerError);
#endif                                      // ENABLE_SOMEIP_DELIVERY_STATISTICS
        } catch (const std::exception& e) { // when app is shut down after offering.
            LGSOMEIP_LOG_INFO << "PacketRouterProxy::on_message() got " << e.what() << ", "
                              << format_named_id("MessageID", someip_message->get_message_id(), 8) << ", "
                              << format_named_id("RequestID", someip_message->get_request_id(), 8) << ", "
                              << format_named_id("MessageType", someip_message->get_message_type(), 2);
            auto response = MessageBuilder::create_response_message(*someip_message);
            if (response != nullptr) {
                response->set_return_code(SOMEIP_RETURN_CODE::E_NOT_READY);
                response->set_payload(nullptr, 0);
                send_message(response);
            }

#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
            SomeipPacketStatistics::get_instance().increase_dropped_packet(
                SomeipPacketStatistics::kIncoming, someip_message->get_message_id(),
                SomeipPacketStatistics::kApplicationManagerException);
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS
        }
    }
}

void PacketRouterProxy::on_disconnect(std::shared_ptr<Endpoint> endpoint) {
#if !defined(ENABLE_SOMEIP_IPC)
    if (endpoint == listener_) {
        listener_->stop_listen();
        listener_ = nullptr;
        host_->on_application_state(false);

        LGSOMEIP_LOG_INFO << "PacketRouterProxy::on_disconnect / Listener Stop!";
    }
#else
    std::uint32_t fd = endpoint->get_socket()->get_socket_fd();
    LGSOMEIP_LOG_DEBUG << "PacketRouterProxy::on_disconnect / fd = " << fd;

    struct RoutingConnectionInfo& info = connected_endpoints_[fd];
    info.endpoint->decrease_reference_count();
    if (info.endpoint->get_reference_count() == 0) {
        LGSOMEIP_LOG_INFO << "PacketRouterProxy::on_disconnect / Listener Stop!";
        info.endpoint->stop_listen();
        connected_endpoints_.erase(fd);
    }
#endif // ENABLE_SOMEIP_IPC
}

void PacketRouterProxy::start_retry_connect() {
#if defined(ENABLE_QNX_MESSAGE_PASSING)
    if (message_passing_receiver_) {
        auto mpserver = std::static_pointer_cast<EndpointMessagePassingServer>(message_passing_receiver_);
        mpserver->set_timer(LGSOMEIP_RETRY_CONNECT_TIMER_ID, LGSOMEIP_RETRY_CONNECT_TIME, true);
    }
#endif
}

void PacketRouterProxy::stop_retry_connect() {
#if defined(ENABLE_QNX_MESSAGE_PASSING)
    if (message_passing_receiver_) {
        auto mpserver = std::static_pointer_cast<EndpointMessagePassingServer>(message_passing_receiver_);
        mpserver->kill_timer(LGSOMEIP_RETRY_CONNECT_TIMER_ID);
    }
#endif
}

void PacketRouterProxy::do_connect() {
    LGSOMEIP_LOG_DEBUG << "PacketRouterProxy::do_connect";
#if !defined(ENABLE_QNX_MESSAGE_PASSING)
    if (sender_ == nullptr) {
        LGSOMEIP_LOG_INFO << "PacketRouterProxy::do_connect / Try Reconnect!";
        std::shared_ptr<lgsomeip::osabstraction::Socket> send_socket =
            EndpointUtils::create_local_socket<lgsomeip::osabstraction::TCPClientSocket>(SOMEIP_DAEMON_ID, true);

        if (send_socket != nullptr) {
            sender_ = std::make_shared<Endpoint>();
            sender_->set_socket(send_socket);
            do_register_application();
        } else {
            LGSOMEIP_LOG_ERROR << "PacketRouterProxy::do_connect / Connect Fail!";
        }
    }
#else
    if (message_passing_sender_ == nullptr) {
        LGSOMEIP_LOG_INFO << "PacketRouterProxy::do_connect/ Try Reconnect!";
        auto sender = std::make_shared<EndpointMessagePassingSender>(
            EndpointUtils::create_message_passing_channel_name(SOMEIP_DAEMON_ID));

        if (sender->is_connected()) {
            message_passing_sender_ = sender;
            do_register_application();
            stop_retry_connect();
        } else {
            LGSOMEIP_LOG_ERROR << "PacketRouterProxy::do_connect/ Connect Fail!";
        }
    }
#endif
}

void PacketRouterProxy::on_reconnect() {
    if (sender_ == nullptr) {
        LGSOMEIP_LOG_INFO << "PacketRouterProxy::on_reconnect / Try Reconnect!";
        std::shared_ptr<lgsomeip::osabstraction::Socket> send_socket =
            EndpointUtils::create_local_socket<lgsomeip::osabstraction::TCPClientSocket>(SOMEIP_DAEMON_ID, true);
        if (send_socket != nullptr) {
            sender_ = std::make_shared<Endpoint>();
            sender_->set_socket(send_socket);
            do_register_application();
        } else {
            LGSOMEIP_LOG_WARN << "PacketRouterProxy::on_reconnect / Connect Fail!";
        }
    }
}

#if defined(ENABLE_QNX_MESSAGE_PASSING)
#if !defined(ENABLE_SOMEIP_IPC)

void PacketRouterProxy::on_message_passing_timer(const int32_t id) {
    if (id == LGSOMEIP_RETRY_CONNECT_TIMER_ID) {
        do_connect();
    } else {
        LGSOMEIP_LOG_WARN << "PacketRouterProxy::on_message_passing_timer unknown timer(" << (int)id << ") is expired";
    }
}

void PacketRouterProxy::on_message_passing_connect(std::shared_ptr<Endpoint> server_endpoint,
                                                   std::shared_ptr<Endpoint> client_endpoint) {
    LGSOMEIP_LOG_DEBUG << "PacketRouterProxy::onMessagePassingConnect clientEndpoint = "
                       << (void*)client_endpoint.get();
    if (message_passing_receiver_ == server_endpoint) {
        message_passing_listener_ = client_endpoint;
        message_passing_listener_->start_listen();
    }
}

void PacketRouterProxy::on_message_passing_disconnect(std::shared_ptr<Endpoint> endpoint) {
    LGSOMEIP_LOG_DEBUG << "PacketRouterProxy::onMessagePassingDisconnect endpoint =" << (void*)endpoint.get();
    if (endpoint == message_passing_listener_) {
        message_passing_listener_->stop_listen();
        message_passing_listener_ = nullptr;
        message_passing_sender_ = nullptr;

        host_->on_application_state(false);

        LGSOMEIP_LOG_INFO << "PacketRouterProxy::onMessagePassingDisconnect / Listener Stop!";

        LGSOMEIP_LOG_INFO << "PacketRouterProxy::onMessagePassingDisconnect / Try connect!";
        start_retry_connect();
    }
}

#else
void PacketRouterProxy::on_message_passing_connect(std::shared_ptr<Endpoint> server_endpoint,
                                                   std::shared_ptr<Endpoint> client_endpoint, int connection_id) {
    std::lock_guard<std::mutex> lock(mutex_processing_);
    if (server_endpoint == nullptr || client_endpoint == nullptr) {
        LGSOMEIP_LOG_ERROR << "PacketRouterProxy::onMessagePassingConnect";
        return;
    }

    std::uint32_t clientfd = connection_id;

    if (message_passing_receiver_ == server_endpoint) {
        LGSOMEIP_LOG_INFO << "PacketRouterProxy::onMessagePassingConnect"
                          << " clientfd(connection_id) : " << MSGID_FORMAT4(connection_id);
        if (connected_endpoints_.find(clientfd) != connected_endpoints_.end()) {
            struct RoutingConnectionInfo& info = connected_endpoints_[clientfd];
            info.endpoint->increase_reference_count();
        } else {
            struct RoutingConnectionInfo info;
            info.endpoint = client_endpoint;
            info.is_internal = true;
            info.app_id = 0;
            info.endpoint->increase_reference_count();
            connected_endpoints_[clientfd] = info;
            client_endpoint->start_listen();
        }
    } else {
        LGSOMEIP_LOG_ERROR << "PacketRouterProxy::onMessagePassingConnect / Invalid Case";
    }
}

void PacketRouterProxy::on_message_passing_disconnect(std::shared_ptr<Endpoint> endpoint, int connection_id) {
    std::lock_guard<std::mutex> lock(mutex_processing_);

    LGSOMEIP_LOG_INFO << "PacketRouterProxy::onMessagePassingDisconnect / "
                      << format_named_id("ConnectionID", static_cast<std::uint32_t>(connection_id), 4);
    if (connected_endpoints_.find(connection_id) != connected_endpoints_.end()) {
        struct RoutingConnectionInfo& info = connected_endpoints_[connection_id];
        info.endpoint->decrease_reference_count();
        if (info.endpoint->get_reference_count() == 0) {
            LGSOMEIP_LOG_INFO << "PacketRouterProxy::onMessagePassingDisconnect / Listener Stop!";
            info.endpoint->stop_listen();
            connected_endpoints_.erase(connection_id);
        }
    }
}
#endif // ENABLE_SOMEIP_IPC

void PacketRouterProxy::on_message_passing_message(std::shared_ptr<Endpoint> endpoint, std::uint8_t* message,
                                                   std::size_t message_length) {
    on_message(endpoint, message, message_length);
}
#endif // ENABLE_QNX_MESSAGE_PASSING

void PacketRouterProxy::send_message(std::shared_ptr<MessageSD> message) {
    std::lock_guard<std::mutex> lock(send_message_mutex_);

    LGSOMEIP_LOG_DEBUG << "PacketRouterProxy::send_message / SOME/IP-SD";

    std::uint32_t length;

    // Serialize SOME/IP-SD messages into a static buffer protected by
    // send_message_mutex_.
    MessageBuilder::build_byte_stream(send_buffer_someip_sd_, &length, *message);

    if (sender_ != nullptr) {
        do_send_message(sender_, send_buffer_someip_sd_, length);
    } else if (message_passing_sender_ != nullptr) {
        do_send_message(message_passing_sender_, send_buffer_someip_sd_, length);
    }
}

void PacketRouterProxy::send_message(std::shared_ptr<MessageSOMEIP> message) {
    send_message(*message);
}

void PacketRouterProxy::send_message(MessageSOMEIP& message) {
    std::lock_guard<std::mutex> lock(send_message_mutex_);

    LGSOMEIP_LOG_DEBUG << "PacketRouterProxy::send_message / SOME/IP";

    std::vector<std::uint8_t> send_buffer(SOMEIP_HEADER::SIZE);
    std::uint8_t* payload_byte = send_buffer.data();
    std::uint32_t length_header;
    std::uint32_t length_message;
    std::uint16_t instance_id;

    auto& payload_vector = message.get_payload_type()->get_payload_vector();

    MessageBuilder::build_byte_stream_some_ip_header(payload_byte, &length_header, message);

    // Append the payload to the SOME/IP header buffer.
    send_buffer.insert(std::end(send_buffer), std::begin(payload_vector), std::end(payload_vector));

    // Append the instance ID for delivery to the SOME/IP daemon.
    send_buffer.insert(std::end(send_buffer), 2, 0x00);
    payload_byte = send_buffer.data();
    instance_id = message.get_instance_id();
    set_byte_stream(payload_byte + send_buffer.size() - 2, &instance_id, 2);

    length_message = send_buffer.size();

    if (sender_ != nullptr) {
        do_send_message(sender_, payload_byte, length_message);
    } else if (message_passing_sender_ != nullptr) {
        do_send_message(message_passing_sender_, payload_byte, length_message);
    }

#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
    SomeipPacketStatistics::get_instance().increase_delivered_packet(SomeipPacketStatistics::kOutgoing,
                                                                     message.get_message_id());
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS

    LGSOMEIP_LOG_DEBUG << "PacketRouterProxy::send_message / length = " << length_message << ", "
                       << format_named_id("InstanceID", instance_id, 4);
}

#if defined(ENABLE_SOMEIP_IPC)
void PacketRouterProxy::send_ipc_response_message(MessageSOMEIP& message, std::uint16_t request_id,
                                                  std::uint16_t app_id) {
    std::uint32_t length;
    std::uint16_t service_id = 0;
    std::vector<std::uint8_t> buffer(EndpointBase::MAX_TRANSFER_PACKET_LENGTH);
    MessageBuilder::build_byte_stream(buffer.data(), &length, message);
    std::uint16_t instance_id = message.get_instance_id();

    // Reuse the EventManager notification payload buffer during cyclic updates.
    if (ipc_senders_.find(app_id) == ipc_senders_.end()) {
        throw LSAR_APPLICATION_ERROR(SOMEIP_RETURN_CODE::E_UNKNOWN_SERVICE);
    } else {
        set_byte_stream(buffer.data() + length, &instance_id, 2);
        length += 2;
        service_id = static_cast<std::uint16_t>(message.get_message_id() >> 16);
        LGSOMEIP_LOG_DEBUG << "PacketRouterProxy::send_ipc_response_message / "
                           << format_service_instance_id(service_id, instance_id);

        if (ipc_senders_.find(app_id) != ipc_senders_.end()) {
            LGSOMEIP_LOG_DEBUG << "PacketRouterProxy::send_ipc_response_message / do_send_message";
            do_send_message(ipc_senders_[app_id], buffer.data(), length);
        } else {
            LGSOMEIP_LOG_ERROR << "PacketRouterProxy::send_ipc_response_message / Cannot found IPC sender for "
                               << format_named_id("AppID", app_id, 4);
        }
    }
}

void PacketRouterProxy::send_ipc_message(std::shared_ptr<MessageSOMEIP> message) {
    if (message != nullptr) {
        send_ipc_message(*message, message->get_message_type() == SOMEIP_MESSAGE_TYPE::REQUEST);
    }
}

void PacketRouterProxy::send_ipc_message(MessageSOMEIP& message, bool is_provider) {
    std::uint32_t length;
    std::uint16_t service_id = 0;
    std::vector<std::uint8_t> buffer(EndpointBase::MAX_TRANSFER_PACKET_LENGTH);
    MessageBuilder::build_byte_stream(buffer.data(), &length, message);
    std::uint16_t instance_id = message.get_instance_id();
    std::uint16_t method_id = static_cast<std::uint16_t>(message.get_message_id() & 0xffff);

    set_byte_stream(buffer.data() + length, &instance_id, 2);
    length += 2;
    service_id = static_cast<std::uint16_t>(message.get_message_id() >> 16);

    LGSOMEIP_LOG_DEBUG << "PacketRouterProxy::send_ipc_message / "
                       << format_service_instance_event_id(service_id, instance_id, method_id)
                       << (is_provider ? " Provider" : " Consumer");

    if (find_ipc_connection_info_service_id(service_id)) {
        if (find_ipc_connection_info_instance_id(service_id, instance_id)) {
            if (find_ipc_connection_info_event_group_id(service_id, instance_id, method_id)) {
                for (auto ipc_sender : ipc_connection_info_[service_id][instance_id][method_id]) {
                    if (ipc_sender.first == is_provider) {
                        LGSOMEIP_LOG_DEBUG << "PacketRouterProxy::sendIpcMessage / do_send_message";
                        do_send_message(ipc_sender.second, buffer.data(), length);
                    }
                }
            } else {
                if (message.get_message_type() == SOMEIP_MESSAGE_TYPE::REQUEST) {
                    if (find_ipc_connection_info_event_group_id(service_id, instance_id, 0xffff)) {
                        for (auto ipc_sender : ipc_connection_info_[service_id][instance_id][0xffff]) {
                            if (ipc_sender.first == is_provider) {
                                LGSOMEIP_LOG_DEBUG << "PacketRouterProxy::sendIpcMessage / do_send_message (REQUEST)";
                                do_send_message(ipc_sender.second, buffer.data(), length);
                                break;
                            }
                        }
                    }
                } else {
                    LGSOMEIP_LOG_ERROR << "PacketRouterProxy::sendIpcMessage / Cannot found IPC sender for method ID";
                }
            }
        } else {
            LGSOMEIP_LOG_ERROR << "PacketRouterProxy::sendIpcMessage / Cannot found IPC sender for instance ID";
        }
    } else {
        LGSOMEIP_LOG_ERROR << "PacketRouterProxy::sendIpcMessage / Cannot found IPC sender for service ID";
    }
}

void PacketRouterProxy::on_internal_request(std::uint16_t service_id, std::uint16_t instance_id,
                                            std::uint16_t request_id, std::uint16_t app_id) {
    std::lock_guard<std::mutex> lock(mutex_processing_);
    LGSOMEIP_LOG_DEBUG << "PacketRouterProxy::onInternalRequest " << format_service_instance_id(service_id, instance_id)
                       << " " << format_named_id("RequestID", request_id, 8);

    if (ipc_senders_.find(app_id) == ipc_senders_.end()) {
        throw LSAR_APPLICATION_ERROR(SOMEIP_RETURN_CODE::E_UNKNOWN_SERVICE);
    } else {
        std::uint16_t client_id = static_cast<std::uint16_t>(request_id >> 16);

        LGSOMEIP_LOG_DEBUG << "PacketRouterProxy::on_internal_request / " << format_named_id("AppID", app_id, 4)
                           << " Set IpcReuestMap / " << format_named_id("ClientID", client_id, 4);
        auto& info = ipc_request_map_[client_id];
        info.ttl = 5;
        info.endpoint = ipc_senders_[app_id];
    }
}
#endif // ENABLE_SOMEIP_IPC

// PacketRouterProxy private methods.

std::shared_ptr<MessageSD> PacketRouterProxy::compose_application_register(bool register_application) {
    // Composing SOME/IP-SD Message
    std::shared_ptr<MessageSD> message = MessageBuilder::create<SOMEIPSD>();
    std::uint32_t req_id = (host_->get_application_id() & 0xffff) << 16;
    message->set_request_id(req_id);

    SDOption option[2]{SOMEIP_SD_OPTION::CONFIGURATION::TYPEID, 0};
    option[0].set_configuration(std::string(SOMEIP_APPLICATION_NAME), host_->get_application_name());

    SDEntry entry(SOMEIP_SD_ENTRY::INTERNAL::TYPEID);
    entry.set_option1st_count(1);
    entry.set_ttl(register_application ? SOMEIP_SD_ENTRY::INTERNAL::REGISTER : SOMEIP_SD_ENTRY::INTERNAL::DEREGISTER);

    MessageComposer::add_entry(message, &entry, option);

    return message;
}

void PacketRouterProxy::do_register_application() {
    LGSOMEIP_LOG_INFO << "PacketRouterProxy::do_register_application";

    // Composing SOME/IP-SD Message
    std::shared_ptr<MessageSD> message = compose_application_register(true);

    send_message(message);

    host_->on_application_state(true);
}

void PacketRouterProxy::do_un_register_application() {
    LGSOMEIP_LOG_INFO << "PacketRouterProxy::do_un_register_application";

    // Composing SOME/IP-SD Message
    std::shared_ptr<MessageSD> message = compose_application_register(false);

    send_message(message);
    host_->on_application_state(false);
}

void PacketRouterProxy::do_send_message(std::shared_ptr<Endpoint> endpoint, std::uint8_t* message,
                                        std::size_t message_length) {
    if (endpoint == nullptr) {
        LGSOMEIP_LOG_INFO << "PacketRouterProxy::do_send_message / endpoint is nullptr";

#if defined(ENABLE_SOMEIP_DELIVERY_STATISTICS)
        std::uint32_t message_id;
        get_byte_stream(&message_id, message);
        SomeipPacketStatistics::get_instance().increase_dropped_packet(
            SomeipPacketStatistics::kOutgoing, message_id, SomeipPacketStatistics::kPacketRouterProxyEndpointNullptr);
#endif // ENABLE_SOMEIP_DELIVERY_STATISTICS

        return;
    }

    print_byte_message("PacketRouterProxy::do_send_message", message, message_length);

    endpoint->send_message(message, message_length);
}

} // namespace lgsomeip
