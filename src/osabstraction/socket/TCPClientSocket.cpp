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

#include "utils/log/logger.h"
#include "TCPClientSocket.h"

namespace lgsomeip {
namespace osabstraction {

TCPClientSocket::TCPClientSocket(std::shared_ptr<Address> server_address, std::shared_ptr<Address> client_address,
                                 const SecureConfig& secure_config)
    : TCPSocket(client_address, server_address, secure_config) {
    LGSOMEIP_LOG_DEBUG << "TCPClientSocket::TCPClientSocket / fd : " << this->socket_fd_;

    bind();
    connect();

    if (server_address->get_type() != AF_UNIX) {
        // Nagle Algoritm Off
        int option = 1;

        if (::setsockopt(get_socket_fd(), IPPROTO_TCP, TCP_NODELAY, &option, sizeof(option))) {
            LGSOMEIP_LOG_ERROR
                << "TCPClientSocket::TCPClientSocket / Fail to set options associated with a socket. error#"
                << strerror(errno);
        }
    }
}

// Constructor delegation
TCPClientSocket::TCPClientSocket(std::shared_ptr<Address> server_address, const SecureConfig& secure_config)
    : TCPClientSocket(server_address, nullptr, secure_config) {}

int TCPClientSocket::connect() {
    if (::connect(get_socket_fd(), destination_address_->get_address(), destination_address_->get_address_size()) !=
        0) {
        LGSOMEIP_LOG_ERROR << "TCPClientSocket::connect / Connect failed!";
        close_socket();
        return -1;
    }
    LGSOMEIP_LOG_DEBUG << "TCPClientSocket::connect / Connect OK";

#if defined(ENABLE_TLS)
    if (is_secure_connection()) {
        start_secure_connection(get_socket_fd());
    }
#endif // ENABLE_TLS

    return 0;
}

} // namespace osabstraction
} // namespace lgsomeip
