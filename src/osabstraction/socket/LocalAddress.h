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

#ifndef LG_SOMEIP_OSABSTRACTION_SOCKET_LOCAL_ADDRESS_H
#define LG_SOMEIP_OSABSTRACTION_SOCKET_LOCAL_ADDRESS_H

#include "Address.h"
#include <sys/un.h>

namespace lgsomeip {
namespace osabstraction {

class LocalAddress : public Address {
public:
    LocalAddress();
    virtual ~LocalAddress();

    virtual struct sockaddr* get_address() override;
    virtual int get_address_size() const;
    virtual int get_type() const override;

    virtual std::string get_ip_address() const override;
    virtual void set_ip_address(std::string address) override;
    virtual std::string get_file_path() const override;
    virtual void set_file_path(std::string path) override;
    void create_path(std::string path);

    std::string to_string() override;

    friend bool operator==(const LocalAddress& lhs, const LocalAddress& rhs);

protected:
    int lock_fd_{-1};
    struct sockaddr_un socket_address_ {
        0
    };
};

} // namespace osabstraction
} // namespace lgsomeip

#endif // LG_SOMEIP_OSABSTRACTION_SOCKET_LOCAL_ADDRESS_H
