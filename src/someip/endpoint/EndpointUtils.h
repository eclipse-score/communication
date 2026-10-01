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

#ifndef LG_SOMEIP_PACKETROUTER_ENDPOINT_TYPE_H
#define LG_SOMEIP_PACKETROUTER_ENDPOINT_TYPE_H

#include <cstdint>

#include <algorithm>
#include <exception>
#include <memory>
#include <vector>
#include <sstream>
#include <iomanip>

#include <endpoint/Endpoint.h>
#include <packetrouter/PacketRouterConstant.h>

#include <socket/LocalAddress.h>
#include <socket/TCPClientSocket.h>
#include <socket/TCPServerSocket.h>
#include <socket/UDPSocket.h>
#include <utils/log/logger.h>

namespace lgsomeip {

// -----------------------------------------------------------------------------
// PacketRouter Endpoint Utils : Socket Utils for Endpoint
// -----------------------------------------------------------------------------
class EndpointUtils {
public:
    template <typename SOCKETTYPE>
    static std::shared_ptr<SOCKETTYPE> create_local_socket(std::uint16_t app_id, bool reliable);

    template <typename HOSTTYPE>
    static std::shared_ptr<Endpoint> create_server_endpoint(HOSTTYPE* host,
                                                            std::shared_ptr<lgsomeip::osabstraction::Address> address,
                                                            bool secure_connection = false);

    template <typename HOSTTYPE>
    static std::shared_ptr<Endpoint>
    create_client_endpoint(HOSTTYPE* host, std::shared_ptr<lgsomeip::osabstraction::Address> address,
                           std::shared_ptr<lgsomeip::osabstraction::Address> server_address,
                           bool secure_connection = false);

#if defined(ENABLE_QNX_MESSAGE_PASSING)
    static std::string create_message_passing_channel_name(std::uint16_t app_id);
#endif /* ENABLE_QNX_MESSAGE_PASSING */
};

template <typename SOCKETTYPE>
std::shared_ptr<SOCKETTYPE> EndpointUtils::create_local_socket(std::uint16_t app_id, bool reliable) {
    try {
        std::stringstream socketpath;
        socketpath << SOMEIP_SOCKET_PREFIX << std::setfill('0') << std::setw(4) << std::hex << app_id;
        std::string path = socketpath.str();

        LGSOMEIP_LOG_DEBUG << "EndpointUtils::create_local_socket / " << path;

        std::shared_ptr<lgsomeip::osabstraction::Address> addr =
            std::make_shared<lgsomeip::osabstraction::LocalAddress>();
        addr->set_reliable(reliable);
        addr->set_file_path(path.c_str());

        auto socket = std::make_shared<SOCKETTYPE>(addr);
        if (socket->get_socket_fd() == lgsomeip::osabstraction::kInvalidSocket) {
            return nullptr;
        }

        return socket;
    } catch (const std::exception& e) {
        LGSOMEIP_LOG_ERROR << "EndpointUtils::create_local_socket() get exception " << e.what();
        return nullptr;
    }
}

template <typename HOSTTYPE>
std::shared_ptr<Endpoint>
EndpointUtils::create_server_endpoint(HOSTTYPE* host, std::shared_ptr<lgsomeip::osabstraction::Address> address,
                                      bool secure_connection) {
    std::shared_ptr<Endpoint> new_endpoint = nullptr;
    if (address == nullptr)
        return nullptr;
    if (address->get_reliable()) {
        std::shared_ptr<lgsomeip::osabstraction::TCPServerSocket> new_socket;

#if defined(ENABLE_TLS)
        if (secure_connection) {
            lgsomeip::osabstraction::SecureConfig secure_config = {true, true, true};
            new_socket = std::make_shared<lgsomeip::osabstraction::TCPServerSocket>(address, secure_config);
        } else {
            new_socket = std::make_shared<lgsomeip::osabstraction::TCPServerSocket>(address);
        }
#else  // ENABLE_TLS
        new_socket = std::make_shared<lgsomeip::osabstraction::TCPServerSocket>(address);
#endif // ENABLE_TLS

        if (new_socket->listen() == -1) {
            return nullptr;
        }
        new_endpoint = std::make_shared<EndpointTCPServer<HOSTTYPE>>(host);
        new_endpoint->set_socket(new_socket);
    } else {
        std::shared_ptr<lgsomeip::osabstraction::UDPSocket> new_socket;

#if defined(ENABLE_TLS)
        if (secure_connection) {
            lgsomeip::osabstraction::SecureConfig secure_config = {true, false, true};
            new_socket = std::make_shared<lgsomeip::osabstraction::UDPSocket>(address, secure_config);
        } else {
            new_socket = std::make_shared<lgsomeip::osabstraction::UDPSocket>(address);
        }
#else  // ENABLE_TLS
        new_socket = std::make_shared<lgsomeip::osabstraction::UDPSocket>(address);
#endif // ENABLE_TLS

        if (new_socket->bind() == -1) {
            return nullptr;
        }
        new_endpoint = std::make_shared<EndpointUDP<HOSTTYPE>>(host);
        new_endpoint->set_socket(new_socket);
    }

    return new_endpoint;
}

template <typename HOSTTYPE>
std::shared_ptr<Endpoint>
EndpointUtils::create_client_endpoint(HOSTTYPE* host, std::shared_ptr<lgsomeip::osabstraction::Address> address,
                                      std::shared_ptr<lgsomeip::osabstraction::Address> server_address,
                                      bool secure_connection) {
    std::shared_ptr<Endpoint> new_endpoint = nullptr;

    if (address == nullptr)
        return nullptr;
    if (address->get_reliable()) {
        std::shared_ptr<lgsomeip::osabstraction::TCPClientSocket> new_socket;

#if defined(ENABLE_TLS)
        if (secure_connection) {
            lgsomeip::osabstraction::SecureConfig secure_config = {true, true, false};
            new_socket =
                std::make_shared<lgsomeip::osabstraction::TCPClientSocket>(server_address, address, secure_config);
        } else {
            new_socket = std::make_shared<lgsomeip::osabstraction::TCPClientSocket>(server_address, address);
        }
#else  // ENABLE_TLS
        new_socket = std::make_shared<lgsomeip::osabstraction::TCPClientSocket>(server_address, address);
#endif // ENABLE_TLS

        // When TCP connection fails to open, socket_fd_ is set to kInvalidSocket(-1) by close_socket()
        // Then, ServiceManager tries to reconnect TCP
        if (new_socket->get_socket_fd() == lgsomeip::osabstraction::kInvalidSocket) {
            return nullptr;
        }

        new_endpoint = std::make_shared<EndpointTCPClient<HOSTTYPE>>(host);
        new_endpoint->set_socket(new_socket);
    } else {
        std::shared_ptr<lgsomeip::osabstraction::UDPSocket> new_socket;

#if defined(ENABLE_TLS)
        if (secure_connection) {
            lgsomeip::osabstraction::SecureConfig secure_config = {true, false, false};
            new_socket = std::make_shared<lgsomeip::osabstraction::UDPSocket>(address, secure_config);
        } else {
            new_socket = std::make_shared<lgsomeip::osabstraction::UDPSocket>(address);
        }
#else  // ENABLE_TLS
        new_socket = std::make_shared<lgsomeip::osabstraction::UDPSocket>(address);
#endif // ENABLE_TLS

        if (new_socket->bind() == -1) {
            return nullptr;
        }

#if defined(ENABLE_TLS)
        if (secure_connection) {
            new_socket->start_secure_connection(new_socket->get_socket_fd(), server_address);
        }
#endif // ENABLE_TLS

        new_endpoint = std::make_shared<EndpointUDP<HOSTTYPE>>(host);
        new_endpoint->set_socket(new_socket);
    }

    return new_endpoint;
}

#if defined(ENABLE_QNX_MESSAGE_PASSING)
std::string EndpointUtils::create_message_passing_channel_name(std::uint16_t app_id) {
    std::stringstream channel_name;
    channel_name << SOMEIP_MESSAGEPASSING_CHANNEL_NAME_PREFIX << std::setfill('0') << std::setw(4) << std::hex
                 << app_id;
    return channel_name.str();
}
#endif // ENABLE_QNX_MESSAGE_PASSING

} // namespace lgsomeip

#endif // LG_SOMEIP_PACKETROUTER_ENDPOINT_TYPE_H
