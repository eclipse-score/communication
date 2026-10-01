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

#include <socket/IP4Address.h>
#include <arpa/inet.h>
#include <cstring>
#include <gtest/gtest.h>
#include <iostream>
#include <memory>

using namespace lgsomeip::osabstraction;

TEST(IP4Address, get_address) {
    std::shared_ptr<Address> addr = std::make_shared<IP4Address>();
    int port = 7000;
    addr->set_port_address(port);

    struct sockaddr_in* p_addr = reinterpret_cast<struct sockaddr_in*>(addr->get_address());
    unsigned sin_port = ntohs(p_addr->sin_port);
    EXPECT_EQ(sin_port, port);
}

TEST(IP4Address, set_ip_address) {
    std::shared_ptr<Address> addr = std::make_shared<IP4Address>();
    addr->set_ip_address("127.0.0.1");
    struct sockaddr_in* p_addr = reinterpret_cast<struct sockaddr_in*>(addr->get_address());
    EXPECT_STREQ("127.0.0.1", inet_ntoa(p_addr->sin_addr));
}

TEST(IP4Address, get_ip_address) {
    std::shared_ptr<Address> addr = std::make_shared<IP4Address>();
    addr = std::make_shared<IP4Address>();

    addr->set_ip_address("127.0.0.1");
    EXPECT_EQ("127.0.0.1", addr->get_ip_address()) << "IP : " << addr->get_ip_address();

    addr->set_ip_address("216.111.111.23");
    EXPECT_EQ("216.111.111.23", addr->get_ip_address());
}

TEST(IP4Address, get_type) {
    std::shared_ptr<Address> addr = std::make_shared<IP4Address>();
    EXPECT_EQ(AF_INET, addr->get_type());
}

TEST(IP4Address, get_address_size) {
    std::shared_ptr<Address> addr = std::make_shared<IP4Address>();
    EXPECT_EQ(sizeof(*(addr->get_address())), addr->get_address_size());
}
