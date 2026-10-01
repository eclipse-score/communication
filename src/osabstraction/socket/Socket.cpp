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

#include <sys/un.h>
#include <sys/stat.h>
#include <thread>

#include "utils/log/logger.h"
#include "Socket.h"

namespace lgsomeip {
namespace osabstraction {

Socket::Socket(std::shared_ptr<Address> source_address, std::shared_ptr<Address> destination_address,
               const SecureConfig& secure_config)
    : source_address_(source_address), destination_address_(destination_address)
#if defined(ENABLE_TLS)
      ,
      secure_config_(secure_config)
#endif // ENABLE_TLS
{
    create_socket();

#if defined(ENABLE_TLS)
    if (secure_config_.is_secure_connection) {
        secure_connector_ = std::make_unique<SecureConnector>(secure_config);
    }
#endif // ENABLE_TLS
}

// Constructor delegation
Socket::Socket(std::shared_ptr<Address> source_address, const SecureConfig& secure_config)
    : Socket(source_address, nullptr, secure_config) {}

Socket::Socket(int file_descriptor, std::shared_ptr<Address> destination_address, const SecureConfig& secure_config)
    : source_address_(nullptr), destination_address_(destination_address)
#if defined(ENABLE_TLS)
      ,
      secure_config_(secure_config)
#endif // ENABLE_TLS
{
    socket_fd_ = file_descriptor;

#if defined(ENABLE_TLS)
    if (secure_config_.is_secure_connection) {
        secure_connector_ = std::make_unique<SecureConnector>(secure_config);
    }
#endif // ENABLE_TLS
}

Socket::~Socket() {
    LGSOMEIP_LOG_DEBUG << "Socket::~Socket / [" << this->socket_fd_ << "] was closed";

    close_socket();
    source_address_ = nullptr;
    socket_fd_ = 0;
}

void Socket::create_socket() {
    int domain = 0, type = 0;

    auto addr = (source_address_ != nullptr) ? source_address_ : destination_address_;
    if (addr->get_type() == AF_INET) {
        domain = PF_INET;
    } else if (addr->get_type() == AF_INET6) {
        domain = PF_INET6;
    } else if (addr->get_type() == AF_UNIX) {
        domain = PF_UNIX;
    }

    type = addr->get_reliable() ? SOCK_STREAM : SOCK_DGRAM;
    socket_fd_ = ::socket(domain, type, 0);

    LGSOMEIP_LOG_DEBUG << "Socket::create_socket FD : " << this->socket_fd_;

    if (get_socket_fd() == kInvalidSocket) {
        LGSOMEIP_LOG_ERROR << "Socket::create_socket / Create fail. INVALID_SOCKET";
    }
}

void Socket::close_socket() {
    if (socket_fd_ == kInvalidSocket) {
        LGSOMEIP_LOG_ERROR << "Socket::close_socket / Already Socket Closed";
    } else {
        if (::close(socket_fd_) == kInvalidSocket) {
            LGSOMEIP_LOG_ERROR << "Socket::close_socket / Socket Close Error";
        }

        socket_fd_ = kInvalidSocket;
    }
}

int Socket::set_nonblocking(int fd, bool enable) {
    int flags, ret;
    flags = fcntl(fd, F_GETFL, 0);

    if (flags == -1) {
        LGSOMEIP_LOG_ERROR << "Socket::set_nonblocking / fcntl failed. error#" << errno;
        return -1;
    }

    if (enable) {
        flags |= O_NONBLOCK;
    } else {
        flags &= (~O_NONBLOCK);
    }

    ret = fcntl(fd, F_SETFL, flags);
    if (ret == -1) {
        LGSOMEIP_LOG_ERROR << "Socket::set_nonblocking / set Nonblockmode failed. error#" << errno;
        return -1;
    }

    return 0;
}

int Socket::bind() {
    bool bind_success = false;
    int on = 1;

    if (get_socket_fd() == kInvalidSocket) {
        LGSOMEIP_LOG_ERROR << "Socket::bind / INVALID_SOCKET";
        return -1;
    }

    if (source_address_ == nullptr)
        return 0;

    // Allow socket descriptor to be reuseable.
    if (setsockopt(get_socket_fd(), SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on)) < 0) {
        // TODO Exception!!
        LGSOMEIP_LOG_ERROR << "Socket::bind / SO_REUSEADDR failed!";
        close_socket();
        return -1;
    }

    // Unless socket bind is successful, retry 100 times with the 10ms interval.
    for (int i = 0; i < 100; i++) {
        if (::bind(get_socket_fd(), source_address_->get_address(), source_address_->get_address_size()) == -1) {
            LGSOMEIP_LOG_DEBUG << "Socket::bind / error: " << strerror(errno)
                               << ", get_socket_fd(): " << get_socket_fd()
                               << ", addr: " << source_address_->to_string();
        } else {
            LGSOMEIP_LOG_INFO << "Socket::bind / success" << ", get_socket_fd(): " << get_socket_fd()
                              << ", addr: " << source_address_->to_string();

            bind_success = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (bind_success == false) {
        LGSOMEIP_LOG_ERROR << "Socket::bind / cannot bind!!!, addr: " << source_address_->to_string();
        close_socket();
        return -1;
    }

    // set permission of domain socket
    if (source_address_->get_type() == AF_UNIX) {
        if (chmod(source_address_->get_file_path().c_str(), S_IRUSR | S_IWUSR)) {
            LGSOMEIP_LOG_ERROR << "Socket::bind / Fail to set permission of domain socket. error#" << strerror(errno);
        }
    }

    return 0;
}

int Socket::send(int file_descriptor, const void* buffer, std::size_t size) {
    (void)file_descriptor;
    (void)buffer;
    (void)size;

    LGSOMEIP_LOG_ERROR << "Socket::send / DO NOT call the function of base class!";

    return -1;
}

int Socket::receive(char* buffer, std::size_t size) {
    (void)buffer;
    (void)size;

    LGSOMEIP_LOG_ERROR << "Socket::receive / DO NOT call the function of base class!";

    return -1;
}

void Socket::send(const void* buffer, std::size_t size, std::shared_ptr<Address> to_address) {
    (void)buffer;
    (void)size;
    (void)to_address;

    LGSOMEIP_LOG_ERROR << "Socket::send / DO NOT call the function of base class!";
}

int Socket::receive(char* buffer, std::size_t size, std::shared_ptr<Address> from_address) {
    (void)buffer;
    (void)size;
    (void)from_address;

    LGSOMEIP_LOG_ERROR << "Socket::receive / DO NOT call the function of base class!";

    return -1;
}

AddressType Socket::get_address_type() const {
    auto addr = (source_address_ != nullptr) ? source_address_ : destination_address_;

    if (addr == nullptr)
        return AddressType::None;

    if (typeid(*addr) == typeid(LocalAddress))
        return AddressType::UnixDomain;
    if (typeid(*addr) == typeid(IP4Address))
        return AddressType::IPv4;
    if (typeid(*addr) == typeid(IP6Address))
        return AddressType::IPv6;

    return AddressType::None;
}

bool Socket::get_reliable() const {
    if (source_address_ != nullptr)
        return source_address_->get_reliable();
    return destination_address_->get_reliable();
}

#if defined(ENABLE_TLS)
void Socket::start_secure_connection(const int& file_descriptor, std::shared_ptr<Address> peer_address) {
    if (!is_secure_connection()) {
        LGSOMEIP_LOG_ERROR << "Socket::start_secure_connection / Secure connection is NOT enabled on FD: "
                           << file_descriptor;
        return;
    }

    if (get_reliable()) {
        secure_connector_->start_tls(file_descriptor);
    } else {
        secure_connector_->start_dtls(file_descriptor, peer_address);
    }
}
#endif // ENABLE_TLS

} // namespace osabstraction
} // namespace lgsomeip
