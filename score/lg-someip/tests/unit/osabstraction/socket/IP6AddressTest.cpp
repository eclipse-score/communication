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

#include <arpa/inet.h>
#include <gtest/gtest.h>
#include <memory>

#include <socket/IP6Address.h>

using namespace lgsomeip::osabstraction;

TEST(IP6Address, get_address) {
    auto address = std::make_shared<IP6Address>();
    address->set_port_address(7000);

    auto* socket_address = reinterpret_cast<sockaddr_in6*>(address->get_address());
    EXPECT_EQ(ntohs(socket_address->sin6_port), 7000);
    EXPECT_EQ(socket_address->sin6_family, AF_INET6);
}

TEST(IP6Address, set_ip_address) {
    auto address = std::make_shared<IP6Address>();
    address->set_ip_address("::1");

    char buffer[INET6_ADDRSTRLEN] = {};
    auto* socket_address = reinterpret_cast<sockaddr_in6*>(address->get_address());
    ASSERT_NE(inet_ntop(AF_INET6, &socket_address->sin6_addr, buffer, sizeof(buffer)), nullptr);
    EXPECT_STREQ("::1", buffer);
}

TEST(IP6Address, get_ip_address) {
    auto address = std::make_shared<IP6Address>();
    address->set_ip_address("2001:db8::1");

    EXPECT_EQ(address->get_ip_address(), "2001:db8::1");
}

TEST(IP6Address, get_type) {
    auto address = std::make_shared<IP6Address>();

    EXPECT_EQ(address->get_type(), AF_INET6);
}

TEST(IP6Address, get_address_size) {
    auto address = std::make_shared<IP6Address>();

    EXPECT_EQ(address->get_address_size(), sizeof(sockaddr_in6));
}
