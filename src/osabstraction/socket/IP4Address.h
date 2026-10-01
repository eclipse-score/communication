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

#ifndef LG_SOMEIP_OSABSTRACTION_SOCKET_IP4ADDRESS
#define LG_SOMEIP_OSABSTRACTION_SOCKET_IP4ADDRESS

#include "Address.h"

namespace lgsomeip {
namespace osabstraction {

class IP4Address : public Address {
public:
    IP4Address();
    virtual ~IP4Address();

    virtual struct sockaddr* get_address() override;
    virtual int get_address_size() const;
    virtual int get_type() const override;

    virtual std::string get_ip_address() const override;
    virtual void set_ip_address(std::string address) override;

    std::string to_string() override;

    friend bool operator==(const IP4Address& left, const IP4Address& right);

protected:
    struct sockaddr_in socket_address_ {
        0
    };
};

} // namespace osabstraction
} // namespace lgsomeip

#endif // LG_SOMEIP_OSABSTRACTION_SOCKET_IP4ADDRESS
