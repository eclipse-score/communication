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
#include "NetworkDevice.h"
#include "UDPSocket.h"

namespace lgsomeip {
namespace osabstraction {

UDPSocket::UDPSocket(std::shared_ptr<Address> address, const SecureConfig& secure_config)
    : Socket(address, secure_config) {
    if (address->get_type() == AF_INET) {
        struct in_addr local_interface;
        local_interface.s_addr = inet_addr(NetworkDevice::instance().get_device_addr().c_str());
        if (setsockopt(get_socket_fd(), IPPROTO_IP, IP_MULTICAST_IF, (char*)&local_interface, sizeof(local_interface)) <
            0) {
            LGSOMEIP_LOG_ERROR << "UDPSocket::UDPSocket / Setting IP4 interface error in UDPSocket";
        }
    } else if (address->get_type() == AF_INET6) {
        int ifidx = NetworkDevice::instance().get_device_id();
        if (setsockopt(get_socket_fd(), IPPROTO_IPV6, IPV6_MULTICAST_IF, &ifidx, sizeof(ifidx)) < 0) {
            LGSOMEIP_LOG_ERROR << "UDPSocket::UDPSocket / Setting IP6 interface error in UDPSocket";
        }
    }

    uint8_t int_vlan_prio = address->get_vlan_priority();
    if (int_vlan_prio != 0xff) {
        // Set VLAN_Priority
        LGSOMEIP_LOG_DEBUG << "Set VLAN_Priority in UDPSocket";

#if defined(LINUX)
        int32_t prio = static_cast<int32_t>(int_vlan_prio);
        if (setsockopt(get_socket_fd(), SOL_SOCKET, SO_PRIORITY, &prio, sizeof(prio))) {
            LGSOMEIP_LOG_ERROR << "UDPSocket::UDPSocket / Fail to set vlan priority with a socket. error#"
                               << strerror(errno);
        }
#elif defined(QNX)
        unsigned char prio = static_cast<unsigned char>(int_vlan_prio);
        if (setsockopt(get_socket_fd(), SOL_SOCKET, SO_VLANPRIO, &prio, sizeof(prio))) {
            LGSOMEIP_LOG_ERROR << "UDPSocket::UDPSocket / Fail to set vlan priority with a socket. error#"
                               << strerror(errno);
        }
#endif
    }
}

void UDPSocket::send(const void* buffer, std::size_t size, std::shared_ptr<Address> to_address) {
#if defined(ENABLE_TLS)
    if (is_secure_connection()) {
        LGSOMEIP_LOG_DEBUG << "UDPSocket::send DTLS packet";
        secure_connector_->send(buffer, size, to_address->get_ip_address(), to_address->get_port_address());
    } else {
        if (::sendto(get_socket_fd(), buffer, size, 0, to_address->get_address(), to_address->get_address_size()) ==
            -1) {
            LGSOMEIP_LOG_ERROR << "UDPSocket::send / sendto failed. error#" << errno;
        }
    }
#else  // ENABLE_TLS
    if (::sendto(get_socket_fd(), buffer, size, 0, to_address->get_address(), to_address->get_address_size()) == -1) {
        LGSOMEIP_LOG_ERROR << "UDPSocket::send / sendto failed. error#" << errno;
    }
#endif // ENABLE_TLS
}

int UDPSocket::receive(char* buffer, std::size_t size, std::shared_ptr<Address> from_address) {
    int str_len = -1;

#if defined(ENABLE_TLS)
    if (is_secure_connection()) {
        str_len = receive_dtls_packet(buffer, size, from_address);
    } else {
        str_len = receive_plain_packet(buffer, size, from_address);
    }
#else  // ENABLE_TLS
    str_len = receive_plain_packet(buffer, size, from_address);
#endif // ENABLE_TLS
    return str_len;
}

int UDPSocket::receive_plain_packet(char* buffer, std::size_t size, std::shared_ptr<Address> from_address) {
    int str_len = -1;
    auto my_addr = source_address_;

    if (my_addr == nullptr) {
        LGSOMEIP_LOG_ERROR << "UDPSocket::receive_plain_packet / source_address_(my addr) is not set!";
        return -1;
    }

    switch (my_addr->get_type()) {
    case AF_INET: // IPv4
    {
        sockaddr_in peer_addr = {0};
        socklen_t peer_addr_size = sizeof(peer_addr);

        str_len = ::recvfrom(get_socket_fd(), buffer, size, 0, (sockaddr*)&peer_addr, &peer_addr_size);

        LGSOMEIP_LOG_DEBUG << "UDPSocket::receive_plain_packet / AF_INET Sender IP : " << inet_ntoa(peer_addr.sin_addr)
                           << ", PORT : " << ntohs(peer_addr.sin_port);

        if (from_address != nullptr) {
            from_address->set_ip_address(inet_ntoa(peer_addr.sin_addr));
            from_address->set_port_address(ntohs(peer_addr.sin_port));
            from_address->set_reliable(false);
        }

        break;
    }
    case AF_INET6: // IPv6
    {
        sockaddr_in6 peer_addr = {0};
        socklen_t peer_addr_size = sizeof(peer_addr);

        str_len = ::recvfrom(get_socket_fd(), buffer, size, 0, (sockaddr*)&peer_addr, &peer_addr_size);

        char str[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &peer_addr.sin6_addr, str, sizeof(str));

        LGSOMEIP_LOG_DEBUG << "UDPSocket::receive_plain_packet / AF_INET6 Sender IP : " << str
                           << ", PORT : " << ntohs(peer_addr.sin6_port);

        if (from_address != nullptr) {
            from_address->set_ip_address(str);
            from_address->set_port_address(ntohs(peer_addr.sin6_port));
            from_address->set_reliable(false);
        }

        break;
    }
    case AF_UNIX: // UNIX domain socket
    {
        sockaddr_un peer_addr = {
            0,
        };
        socklen_t peer_addr_size = sizeof(peer_addr);

        str_len = ::recvfrom(get_socket_fd(), buffer, size, 0, (sockaddr*)&peer_addr, &peer_addr_size);

        if (from_address != nullptr) {
            from_address->set_file_path(peer_addr.sun_path);
            from_address->set_reliable(false);
        }

        break;
    }
    default: {
        LGSOMEIP_LOG_ERROR << "UDPSocket::receive_plain_packet / Unexpected address type!";

        return str_len;
    }
    }

    if (str_len < 0) {
        LGSOMEIP_LOG_DEBUG << "UDPSocket::receive_plain_packet / recvfrom failed";
    }

    return str_len;
}

bool UDPSocket::join_multicast(const std::string& group) {
    if (source_address_->get_type() == AF_INET) {
        struct ip_mreq multicast_join;
        if (inet_pton(AF_INET, group.c_str(), &multicast_join.imr_multiaddr) != 1) {
            return false;
        }
        multicast_join.imr_interface.s_addr = inet_addr(NetworkDevice::instance().get_device_addr().c_str());

        if (setsockopt(get_socket_fd(), IPPROTO_IP, IP_ADD_MEMBERSHIP, (void*)&multicast_join, sizeof(multicast_join)) <
            0) {
            LGSOMEIP_LOG_ERROR << "UDPSocket::join_multicast / error#" << strerror(errno);
            return false;
        }
    } else if (source_address_->get_type() == AF_INET6) {
        struct ipv6_mreq multicast_join;
        if (inet_pton(AF_INET6, group.c_str(), (void*)&multicast_join.ipv6mr_multiaddr) != 1) {
            return false;
        }
        multicast_join.ipv6mr_interface = NetworkDevice::instance().get_device_id();

        if (setsockopt(get_socket_fd(), IPPROTO_IPV6, IPV6_JOIN_GROUP, (void*)&multicast_join, sizeof(multicast_join)) <
            0) {
            LGSOMEIP_LOG_ERROR << "UDPSocket::join_multicast / error#" << strerror(errno);
            return false;
        }
    } else {
        return false;
    }

    set_multicast_loop(0); // Loopback off
    return true;
}

void UDPSocket::leave_multicast(const std::string& group) {
    if (source_address_->get_type() == AF_INET) {
        struct ip_mreq mreq;
        mreq.imr_multiaddr.s_addr = inet_addr(group.c_str());
        mreq.imr_interface.s_addr = htonl(INADDR_ANY);

        if (setsockopt(get_socket_fd(), IPPROTO_IP, IP_DROP_MEMBERSHIP, (void*)&mreq, sizeof(mreq)) < 0) {
            LGSOMEIP_LOG_ERROR << "UDPSocket::join_multicast / error#" << strerror(errno);
            return;
        }
    } else if (source_address_->get_type() == AF_INET6) {
        struct ipv6_mreq mreq;
        inet_pton(AF_INET6, group.c_str(), (void*)&mreq.ipv6mr_multiaddr);
        mreq.ipv6mr_interface = 0;

        if (setsockopt(get_socket_fd(), IPPROTO_IPV6, IPV6_LEAVE_GROUP, (void*)&mreq, sizeof(mreq)) < 0) {
            LGSOMEIP_LOG_ERROR << "UDPSocket::join_multicast / error#" << strerror(errno);
            return;
        }
    }
}

void UDPSocket::set_multicast_ttl(const std::uint32_t ttl) {
    if (source_address_->get_type() == AF_INET) {
        if (setsockopt(get_socket_fd(), IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0) {
            LGSOMEIP_LOG_ERROR << "UDPSocket::set_multicast_ttl / error#" << strerror(errno);
        }
    } else if (source_address_->get_type() == AF_INET6) {
        if (setsockopt(get_socket_fd(), IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &ttl, sizeof(ttl)) < 0) {
            LGSOMEIP_LOG_ERROR << "UDPSocket::set_multicast_ttl / error#" << strerror(errno);
        }
    }
}

void UDPSocket::set_multicast_loop(const std::uint32_t on) {
    if (source_address_->get_type() == AF_INET) {
        if (setsockopt(get_socket_fd(), IPPROTO_IP, IP_MULTICAST_LOOP, &on, sizeof(on)) < 0) {
            LGSOMEIP_LOG_ERROR << "UDPSocket::set_multicast_loop / error#" << strerror(errno);
        }

    } else if (source_address_->get_type() == AF_INET6) {
        if (setsockopt(get_socket_fd(), IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &on, sizeof(on)) < 0) {
            LGSOMEIP_LOG_ERROR << "UDPSocket::set_multicast_loop / error#" << strerror(errno);
        }
    }
}

#if defined(ENABLE_TLS)
int UDPSocket::receive_dtls_packet(char* buffer, std::size_t size, std::shared_ptr<Address> from_address) {
    int str_len = -1;
    std::string addr = "";
    std::uint16_t port = 0;
    auto my_address = source_address_;
    bool is_available = false;

    if (my_address == nullptr) {
        LGSOMEIP_LOG_ERROR << "UDPSocket::receive_dtls_packet / source_address_ is not set!";
        return -1;
    }

    switch (my_address->get_type()) {
    case AF_INET: // IPv4
    {
        sockaddr_in peer_address = {0};

        is_available = SecureConnector::check_dtls_data_available(get_socket_fd(), (sockaddr*)&peer_address);
        addr = inet_ntoa(peer_address.sin_addr);
        port = ntohs(peer_address.sin_port);

        LGSOMEIP_LOG_DEBUG << "UDPSocket::receive_dtls_packet / AF_INET Sender IP : " << addr << ", PORT : " << port;

        if (from_address != nullptr) {
            from_address->set_ip_address(addr);
            from_address->set_port_address(port);
            from_address->set_reliable(false);
        }

        break;
    }
    case AF_INET6: // IPv6
    {
        sockaddr_in6 peer_address = {0};

        is_available = SecureConnector::check_dtls_data_available(get_socket_fd(), (sockaddr*)&peer_address);

        char str[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &peer_address.sin6_addr, str, sizeof(str));
        addr = str;
        port = ntohs(peer_address.sin6_port);

        LGSOMEIP_LOG_DEBUG << "UDPSocket::receive_dtls_packet / AF_INET6 Sender IP : " << addr << ", PORT : " << port;

        if (from_address != nullptr) {
            from_address->set_ip_address(addr);
            from_address->set_port_address(port);
            from_address->set_reliable(false);
        }

        break;
    }
    case AF_UNIX: // UNIX domain socket
    {
        sockaddr_un peer_address = {
            0,
        };

        is_available = SecureConnector::check_dtls_data_available(get_socket_fd(), (sockaddr*)&peer_address);

        addr = peer_address.sun_path;

        if (from_address != nullptr) {
            from_address->set_file_path(addr);
            from_address->set_reliable(false);
        }

        break;
    }
    default: {
        LGSOMEIP_LOG_ERROR << "UDPSocket::receive_dtls_packet / Unexpected address type!";

        return str_len;
    }
    }

    if (!is_available) {
        LGSOMEIP_LOG_ERROR << "UDPSocket::receive_dtls_packet / check_dtls_data_available() returns false";

        return str_len;
    }

    std::string address_key = addr + ":" + std::to_string(port);
    if (secure_connector_->is_new_dtls_connection(addr, port)) {
        if (new_dtls_connection_threads_.find(address_key) == new_dtls_connection_threads_.end()) {
            LGSOMEIP_LOG_DEBUG << "UDPSocket::receive_dtls_packet / Start DTLS handshake with peer Addr:"
                               << address_key;

            new_dtls_connection_threads_.insert(
                std::make_pair(address_key, std::thread(&SecureConnector::start_dtls, secure_connector_.get(),
                                                        get_socket_fd(), from_address)));
            // For assigning thread name
            pthread_setname_np(new_dtls_connection_threads_[address_key].native_handle(), "SomeipDTLSConn");
            new_dtls_connection_threads_[address_key].detach();
        } else {
            LGSOMEIP_LOG_DEBUG << "UDPSocket::receive_dtls_packet / DTLS handshake ongoing with peer Addr:"
                               << address_key;

            std::this_thread::sleep_for(std::chrono::milliseconds(dtls_handshake_wait_ms_));
        }
    } else {
        auto thread_it = new_dtls_connection_threads_.find(address_key);
        if (thread_it != new_dtls_connection_threads_.end()) {
            LGSOMEIP_LOG_DEBUG << "UDPSocket::receive_dtls_packet / handshake done with peer Addr:" << address_key;
            new_dtls_connection_threads_.erase(thread_it);
        } else {
            str_len = secure_connector_->receive(buffer, size, addr, port);
            if (!str_len) {
                secure_connector_->handle_disconnect(addr, port);
            }
        }
    }

    return str_len;
}
#endif // ENABLE_TLS

} // namespace osabstraction
} // namespace lgsomeip
