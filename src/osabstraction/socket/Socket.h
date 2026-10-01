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

#ifndef LG_SOMEIP_OSABSTRACTION_SOCKET_SOCKET
#define LG_SOMEIP_OSABSTRACTION_SOCKET_SOCKET

#include <cstdint>

#include <iostream>
#include <memory>
#include <string>
#include <arpa/inet.h>
#include <netdb.h>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>

#include "Address.h"
#include "IP4Address.h"
#include "IP6Address.h"
#include "LocalAddress.h"
#include "SocketConstant.h"
#include "SecureConnector.h"

namespace lgsomeip {
namespace osabstraction {

enum class AddressType : std::uint16_t { None = 0, UnixDomain = 1, IPv4 = 4, IPv6 = 6 };

class Socket {
public:
    Socket(std::shared_ptr<Address> source_address, std::shared_ptr<Address> destination_address,
           const SecureConfig& secure_config = SecureConfig());
    Socket(std::shared_ptr<Address> source_address, const SecureConfig& secure_config = SecureConfig());
    Socket(int file_descriptor, std::shared_ptr<Address> destination_address,
           const SecureConfig& secure_config = SecureConfig());

    virtual ~Socket();

    void create_socket();
    void close_socket();
    int set_nonblocking(int fd, bool enable);

    int bind();

    // Interface for TCP
    virtual int send(int file_descriptor, const void* buffer, std::size_t size);
    virtual int receive(char* buffer, std::size_t size);

    // Interface for UDP
    virtual void send(const void* buffer, std::size_t size, std::shared_ptr<Address> to_address);
    virtual int receive(char* buffer, std::size_t size, std::shared_ptr<Address> from_address);

    AddressType get_address_type() const;
    bool get_reliable() const;
    inline int get_socket_fd() const {
        return socket_fd_;
    }
    inline std::shared_ptr<Address> get_src_address() const {
        return source_address_;
    }
    inline std::shared_ptr<Address> get_dst_address() const {
        return destination_address_;
    }

protected:
    int socket_fd_;
    bool non_blocking_{false};
    std::shared_ptr<Address> source_address_{nullptr};
    std::shared_ptr<Address> destination_address_{nullptr};
#if defined(ENABLE_TLS)
public:
    inline bool is_secure_connection() const {
        return secure_config_.is_secure_connection;
    }
    void start_secure_connection(const int& file_descriptor, std::shared_ptr<Address> peer_address = nullptr);

protected:
    SecureConfig secure_config_;
    std::unique_ptr<SecureConnector> secure_connector_{nullptr};
#endif // ENABLE_TLS
};

} // namespace osabstraction
} // namespace lgsomeip

#endif // LG_SOMEIP_OSABSTRACTION_SOCKET_SOCKET
