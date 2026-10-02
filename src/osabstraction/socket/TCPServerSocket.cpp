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

#include <errno.h>
#include "utils/log/logger.h"
#include "TCPServerSocket.h"

namespace lgsomeip {
namespace osabstraction {

TCPServerSocket::TCPServerSocket(std::shared_ptr<Address> address, const SecureConfig& secure_config)
    : TCPSocket(address, nullptr, secure_config) {
    LGSOMEIP_LOG_DEBUG << "TCPServerSocket::TCPServerSocket / fd : " << this->socket_fd_;

    uint8_t int_vlan_prio = address->get_vlan_priority();
    if (int_vlan_prio != 0xff) {
        // Set VLAN_Priority
        LGSOMEIP_LOG_DEBUG << "Set VLAN_Priority in TCPServerSocket";

#if defined(LINUX)
        int32_t prio = static_cast<int32_t>(int_vlan_prio);
        if (setsockopt(get_socket_fd(), SOL_SOCKET, SO_PRIORITY, &prio, sizeof(prio))) {
            LGSOMEIP_LOG_ERROR << "TCPServerSocket::TCPServerSocket / Fail to set vlan priority with a socket. error#"
                               << strerror(errno);
        }
#elif defined(QNX)
        unsigned char prio = static_cast<unsigned char>(int_vlan_prio);
        if (setsockopt(get_socket_fd(), SOL_SOCKET, SO_VLANPRIO, &prio, sizeof(prio))) {
            LGSOMEIP_LOG_ERROR << "TCPServerSocket::TCPServerSocket / Fail to set vlan priority with a socket. error#"
                               << strerror(errno);
        }
#endif
    }
    bind();

    if (address->get_type() != AF_UNIX) {
        int option = 1; // Nagle Algoritm Off

        if (setsockopt(get_socket_fd(), IPPROTO_TCP, TCP_NODELAY, &option, sizeof(option))) {
            LGSOMEIP_LOG_ERROR
                << "TCPServerSocket::TCPServerSocket / Fail to set options associated with a socket. error#"
                << strerror(errno);
        }
    }
}

int TCPServerSocket::listen() {
    if (::listen(get_socket_fd(), kDefaultBacklog) != 0) {
        LGSOMEIP_LOG_ERROR << "TCPServerSocket::listen / listen failed!";
        close_socket();
        return -1;
    }

    return 0;
}

std::shared_ptr<Socket> TCPServerSocket::accept() {
    if (get_socket_fd() == kInvalidSocket) {
        LGSOMEIP_LOG_ERROR << "TCPServerSocket::acceptTCPServer / kInvalidSocket";
        // TODO(lg-someip): Throw a socket exception for an invalid listening socket.
    }

    std::shared_ptr<Address> addr{nullptr};
    socklen_t client_addr_size{0};

    int new_socket = ::accept(get_socket_fd(), NULL, NULL);
    if (new_socket == kInvalidSocket) {
        LGSOMEIP_LOG_ERROR << "TCPServerSocket::accept / accept failed";
        // TODO(lg-someip): Throw a socket exception when accept() fails.
    }

    LGSOMEIP_LOG_DEBUG << "TCPServerSocket::accept / newSocket FD : " << new_socket;

    if (get_src_address()->get_type() == AF_INET6) {
        // Case : IPv6
        sockaddr_in6 client_addr{0};
        client_addr_size = sizeof(client_addr);
        char str[INET6_ADDRSTRLEN];

        if (getpeername(new_socket, (struct sockaddr*)&client_addr, &client_addr_size)) {
            LGSOMEIP_LOG_ERROR << "TCPServerSocket::accept / Fail to get name of connected peer socket. error#"
                               << strerror(errno);
        }

        inet_ntop(AF_INET6, &client_addr.sin6_addr, str, sizeof(str));
        addr = std::make_shared<IP6Address>();
        addr->set_ip_address(str);
        addr->set_reliable(true);
        addr->set_port_address(ntohs(client_addr.sin6_port));

        LGSOMEIP_LOG_DEBUG << "TCPServerSocket::accept - AF_INET6 Client IP : " << addr->get_ip_address()
                           << ", Port : " << addr->get_port_address();
    } else if (get_src_address()->get_type() == AF_INET) {
        // Case : IPv4
        sockaddr_in client_addr = {0};
        client_addr_size = sizeof(client_addr);

        if (getpeername(new_socket, (struct sockaddr*)&client_addr, &client_addr_size)) {
            LGSOMEIP_LOG_ERROR << "TCPServerSocket::accept / Fail to get name of connected peer socket. error#"
                               << strerror(errno);
        }
        addr = std::make_shared<IP4Address>();
        addr->set_ip_address(inet_ntoa(client_addr.sin_addr));
        addr->set_reliable(true);
        addr->set_port_address(ntohs(client_addr.sin_port));

        LGSOMEIP_LOG_DEBUG << "TCPServerSocket::accept - AF_INET Client IP : " << addr->get_ip_address()
                           << ", Port : " << addr->get_port_address();
    } else if (get_src_address()->get_type() == AF_UNIX) {
        // Case : Unix Domain Socket
        sockaddr_un client_addr = {0};
        client_addr_size = sizeof(client_addr);

        // Unix-domain (local) sockets do not support getsockname()
        if (getsockname(new_socket, (struct sockaddr*)&client_addr, &client_addr_size)) {
            LGSOMEIP_LOG_ERROR << "TCPServerSocket::accept / Fail to store the current name for the socket. error#"
                               << strerror(errno);
        }
        addr = std::make_shared<LocalAddress>();
        addr->set_file_path(client_addr.sun_path);
        addr->set_reliable(true);

        LGSOMEIP_LOG_DEBUG << "TCPServerSocket::accept - AF_UNIX Client Path : " << client_addr.sun_path;
    }

    std::shared_ptr<TCPSocket> accepted_socket;
#if defined(ENABLE_TLS)
    if (is_secure_connection()) {
        SecureConfig secure_config = {true, true, true};
        accepted_socket = std::make_shared<TCPSocket>(new_socket, addr, secure_config);
        accepted_socket->start_secure_connection(new_socket);
    } else {
        accepted_socket = std::make_shared<TCPSocket>(new_socket, addr);
    }
#else  // ENABLE_TLS
    accepted_socket = std::make_shared<TCPSocket>(new_socket, addr);
#endif // ENABLE_TLS

    return accepted_socket;
}

} // namespace osabstraction
} // namespace lgsomeip
