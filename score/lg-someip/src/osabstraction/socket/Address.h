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

#ifndef LG_SOMEIP_OSABSTRACTION_SOCKET_ADDRESS
#define LG_SOMEIP_OSABSTRACTION_SOCKET_ADDRESS

#include <arpa/inet.h>
#if defined(QNX)
#include <netinet/in.h> // sockaddr_in
#include <sys/socket.h> // sockaddr_storage, AF_*
#endif                  // QNX
#include <cstdint>
#include <string>

namespace lgsomeip {
namespace osabstraction {

class Address {
public:
    Address();
    virtual ~Address();

    virtual struct sockaddr* get_address();
    virtual int get_address_size() const;
    virtual int get_type() const;

    virtual void set_ip_address(std::string address) {}
    virtual std::string get_ip_address() const {
        return std::string();
    }

    virtual void set_file_path(std::string path) {}
    virtual std::string get_file_path() const {
        return std::string();
    }

    void set_reliable(bool reliable);
    bool get_reliable() const;

    void set_port_address(std::uint16_t port);
    std::uint16_t get_port_address() const;

    void set_vlan_priority(std::uint8_t vlan_priority);
    std::uint8_t get_vlan_priority() const;

    bool is_multicast() const;

    virtual std::string to_string();

    friend bool operator==(const Address& left, const Address& right);

protected:
    bool multicast_{false};
    bool reliable_{false};
    std::uint16_t port_{0};
    std::uint8_t vlan_priority_{0xff};
    std::string address_{""};
};

} // namespace osabstraction
} // namespace lgsomeip

#endif // LG_SOMEIP_OSABSTRACTION_SOCKET_ADDRESS
