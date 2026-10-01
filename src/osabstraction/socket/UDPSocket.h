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

#ifndef LG_SOMEIP_OSABSTRACTION_SOCKET_UDPSOCKET
#define LG_SOMEIP_OSABSTRACTION_SOCKET_UDPSOCKET

#include <cstdint>

#include <string>
#include <thread>
#include <map>
#include <chrono>

#include "Address.h"
#include "Socket.h"

namespace lgsomeip {
namespace osabstraction {

class UDPSocket : public Socket {
    const std::uint16_t dtls_handshake_wait_ms_ = 10;

public:
    UDPSocket(std::shared_ptr<Address> address, const SecureConfig& secure_config = SecureConfig());
    ~UDPSocket(){};

    void send(const void* buffer, std::size_t size, std::shared_ptr<Address> to_address) override;
    int receive(char* buffer, std::size_t size, std::shared_ptr<Address> from_address) override;

    bool join_multicast(const std::string& group);
    void leave_multicast(const std::string& group);
    void set_multicast_ttl(const std::uint32_t ttl);
    void set_multicast_loop(const std::uint32_t on);

private:
    int receive_plain_packet(char* buffer, std::size_t size, std::shared_ptr<Address> from_address);
#if defined(ENABLE_TLS)
private:
    int receive_dtls_packet(char* buffer, std::size_t size, std::shared_ptr<Address> from_address);

    std::map<std::string, std::thread> new_dtls_connection_threads_;
#endif // ENABLE_TLS
};

} // namespace osabstraction
} // namespace lgsomeip

#endif // LG_SOMEIP_OSABSTRACTION_SOCKET_UDPSOCKET
