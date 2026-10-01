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
#include "TCPSocket.h"

namespace lgsomeip {
namespace osabstraction {

TCPSocket::TCPSocket(std::shared_ptr<Address> source_address, std::shared_ptr<Address> destination_address,
                     const SecureConfig& secure_config)
    : Socket(source_address, destination_address, secure_config) {
    struct linger {
        int l_onoff;
        int l_linger;
    };
    struct linger linger;

    linger.l_onoff = 1;
    linger.l_linger = 0;

    if (::setsockopt(get_socket_fd(), SOL_SOCKET, SO_LINGER, &linger, sizeof(linger))) {
        LGSOMEIP_LOG_ERROR << "TCPSocket::TCPSocket / Fail to set options associated with a socket. error#"
                           << strerror(errno);
    }
}

TCPSocket::TCPSocket(int file_descriptor, std::shared_ptr<Address> destination_address,
                     const SecureConfig& secure_config)
    : Socket(file_descriptor, destination_address, secure_config) {}

int TCPSocket::send(int file_descriptor, const void* buffer, std::size_t size) {
    std::size_t write_len = 0;

    while (write_len < size) {
        std::size_t ret = 0;
#if defined(ENABLE_TLS)
        if (is_secure_connection()) {
            ret = secure_connector_->send(reinterpret_cast<const std::uint8_t*>(buffer) + write_len, size - write_len);
        } else {
            ret = ::write(file_descriptor, reinterpret_cast<const std::uint8_t*>(buffer) + write_len, size - write_len);
        }
#else  // ENABLE_TLS
        ret = ::write(file_descriptor, reinterpret_cast<const std::uint8_t*>(buffer) + write_len, size - write_len);
#endif // ENABLE_TLS

        if (ret == static_cast<std::size_t>(-1)) {
            LGSOMEIP_LOG_ERROR << "TCPSocket::send / write failed on FD:" << file_descriptor << ", error#"
                               << strerror(errno);
            return -1;
        }

        if (ret == 0) {
            LGSOMEIP_LOG_ERROR << "TCPSocket::send / write returned zero on FD:" << file_descriptor;
            return -1;
        }

        write_len += ret;
    }
    return write_len;
}

int TCPSocket::receive(char* buffer, std::size_t size) {
    if (destination_address_->get_reliable() == false) {
        LGSOMEIP_LOG_ERROR << "TCPSocket::receive / Use UDPSocket::receive!";

        return -1;
    }

    if (non_blocking_ == false) {
        set_nonblocking(socket_fd_, true);
    }

    if (source_address_ != nullptr) {
        if (source_address_->get_type() != AF_UNIX) {
            LGSOMEIP_LOG_DEBUG << "TCPSocket::receive / source_address_.Addr = "
                               << this->source_address_->get_ip_address()
                               << ", source_address_.port = " << this->source_address_->get_port_address();
        } else {
            LGSOMEIP_LOG_DEBUG << "TCPSocket::receive / source_address_.Addr = "
                               << this->source_address_->get_file_path() << "[FD:" << this->socket_fd_
                               << "], size = " << size;
        }
    }

    if (destination_address_ != nullptr) {
        if (destination_address_->get_type() != AF_UNIX) {
            LGSOMEIP_LOG_DEBUG << "TCPSocket::receive / destination_address_.Addr = "
                               << this->destination_address_->get_ip_address()
                               << ", destination_address_.port = " << this->destination_address_->get_port_address();
        } else {
            LGSOMEIP_LOG_DEBUG << "TCPSocket::receive / destination_address_.Addr = "
                               << this->destination_address_->get_file_path() << "[FD:" << this->socket_fd_
                               << "], size = " << size;
        }
    }

    std::size_t read_len = 0;
    do {
        std::size_t ret = 0;
#if defined(ENABLE_TLS)
        if (is_secure_connection()) {
            ret = secure_connector_->receive(buffer + read_len, size - read_len);
        } else {
            ret = ::read(socket_fd_, buffer + read_len, size - read_len);
        }
#else  // ENABLE_TLS
        ret = ::read(socket_fd_, buffer + read_len, size - read_len);
#endif // ENABLE_TLS

        if (ret == static_cast<std::size_t>(-1)) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            LGSOMEIP_LOG_ERROR << "TCPSocket::receive / Socket READ Failed on FD:" << this->socket_fd_ << ", error#"
                               << strerror(errno);
            return -1;
        }

        if (ret == 0) {
            break;
        }

        read_len += ret;
    } while (read_len < size);

    return read_len;
}

} // namespace osabstraction
} // namespace lgsomeip
