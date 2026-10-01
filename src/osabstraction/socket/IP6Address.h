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

#ifndef LG_SOMEIP_OSABSTRACTION_SOCKET_IP6ADDRESS
#define LG_SOMEIP_OSABSTRACTION_SOCKET_IP6ADDRESS

#include "Address.h"

namespace lgsomeip {
namespace osabstraction {

class IP6Address : public Address {
public:
    IP6Address();
    virtual ~IP6Address();

    virtual struct sockaddr* get_address() override;
    virtual int get_address_size() const;
    virtual int get_type() const override;

    virtual std::string get_ip_address() const override;
    virtual void set_ip_address(std::string addr) override;

    std::string to_string() override;

    friend bool operator==(const IP6Address& lhs, const IP6Address& rhs);

protected:
    struct sockaddr_in6 socket_address_ {
        0
    };
};

} // namespace osabstraction
} // namespace lgsomeip

#endif // LG_SOMEIP_OSABSTRACTION_SOCKET_IP6ADDRESS
