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
#include <poll.h>
#include <sys/socket.h>

#include <chrono>

#include "utils/log/logger.h"
#include "TCPSocket.h"

namespace lgsomeip {
namespace osabstraction {

namespace {

// How long send() waits without any progress for a full socket buffer to drain before giving up on a message.
constexpr std::chrono::milliseconds kSendTimeout{1000};

// Wait until file_descriptor can be written, or until deadline. Returns false on timeout or error.
bool wait_writable(int file_descriptor, std::chrono::steady_clock::time_point deadline) {
    for (;;) {
        const auto remaining =
            std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
        if (remaining.count() <= 0) {
            return false;
        }
        pollfd descriptor{file_descriptor, POLLOUT, 0};
        const int ret = ::poll(&descriptor, 1, static_cast<int>(remaining.count()));
        if (ret > 0) {
            return (descriptor.revents & (POLLERR | POLLHUP | POLLNVAL)) == 0;
        }
        if (ret < 0 && errno != EINTR) {
            return false;
        }
    }
}

} // namespace

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
#if defined(ENABLE_TLS)
    if (!is_secure_connection() && !non_blocking_.load()) {
#else  // ENABLE_TLS
    if (!non_blocking_.load()) {
#endif // ENABLE_TLS
        if (set_nonblocking(file_descriptor, true) != 0) {
            LGSOMEIP_LOG_ERROR << "TCPSocket::send / Failed to set FD:" << file_descriptor
                               << " to non-blocking mode";
            return -1;
        }
        non_blocking_ = true;
    }

    std::size_t write_len = 0;
    std::chrono::steady_clock::time_point deadline{};

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
            // The socket is non-blocking (receive() sets O_NONBLOCK), so a full send buffer reports EAGAIN.
            // Wait for it to drain instead of abandoning the message: a SOME/IP message cut off in the middle
            // leaves the peer reading payload bytes as the next header, and the stream never recovers.
            if (errno == EINTR) {
                continue;
            }
            if ((errno == EAGAIN || errno == EWOULDBLOCK)
#if defined(ENABLE_TLS)
                && !is_secure_connection()
#endif // ENABLE_TLS
            ) {
                if (deadline == std::chrono::steady_clock::time_point{}) {
                    deadline = std::chrono::steady_clock::now() + kSendTimeout;
                }
                if (wait_writable(file_descriptor, deadline)) {
                    continue;
                }
                if (write_len == 0) {
                    // Nothing was written, so the stream is still aligned: drop only this message.
                    LGSOMEIP_LOG_ERROR << "TCPSocket::send / send buffer stayed full on FD:" << file_descriptor
                                       << "; message of " << size << " bytes dropped";
                    return -1;
                }
                // Part of the message is already on the wire; the stream cannot be realigned. Close the
                // connection so the peer sees a disconnect instead of a corrupted stream.
                LGSOMEIP_LOG_ERROR << "TCPSocket::send / send buffer stayed full on FD:" << file_descriptor << " after "
                                   << write_len << " of " << size << " bytes; closing the connection";
                ::shutdown(file_descriptor, SHUT_RDWR);
                return -1;
            }
            LGSOMEIP_LOG_ERROR << "TCPSocket::send / write failed on FD:" << file_descriptor << ", error#"
                               << strerror(errno);
            return -1;
        }

        if (ret == 0) {
            LGSOMEIP_LOG_ERROR << "TCPSocket::send / write returned zero on FD:" << file_descriptor;
            return -1;
        }

        write_len += ret;
        // The timeout measures a stall, so progress starts a new wait.
        deadline = std::chrono::steady_clock::time_point{};
    }
    return write_len;
}

int TCPSocket::receive(char* buffer, std::size_t size) {
    if (destination_address_->get_reliable() == false) {
        LGSOMEIP_LOG_ERROR << "TCPSocket::receive / Use UDPSocket::receive!";

        return -1;
    }

    if (!non_blocking_.load()) {
        if (set_nonblocking(socket_fd_, true) != 0) {
            LGSOMEIP_LOG_ERROR << "TCPSocket::receive / Failed to set FD:" << socket_fd_
                               << " to non-blocking mode";
            return -1;
        }
        non_blocking_ = true;
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
