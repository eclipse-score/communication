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

#include <gtest/gtest.h>

#include <endpoint/EndpointBase.h>
#include <socket/IP4Address.h>
#include <socket/Socket.h>

using namespace lgsomeip;
using namespace lgsomeip::osabstraction;

TEST(EndpointBase, stores_metadata) {
    EndpointBase endpoint;

    endpoint.set_instance_id(0x1234);
    endpoint.set_app_id(0x5678);
    endpoint.increase_reference_count();
    endpoint.set_magic_cookie_enabled(true);

    EXPECT_EQ(endpoint.get_instance_id(), 0x1234);
    EXPECT_EQ(endpoint.get_app_id(), 0x5678);
    EXPECT_EQ(endpoint.get_reference_count(), 1);
    EXPECT_TRUE(endpoint.get_magic_cookie_enabled());

    endpoint.decrease_reference_count();
    endpoint.decrease_reference_count();
    EXPECT_EQ(endpoint.get_reference_count(), 0);
}

TEST(EndpointBase, creates_sender_address_for_unreliable_ipv4_socket) {
    auto address = std::make_shared<IP4Address>();
    address->set_reliable(false);
    auto socket = std::make_shared<Socket>(address);
    EndpointBase endpoint(socket);

    ASSERT_NE(endpoint.get_sender_address(), nullptr);
    EXPECT_EQ(endpoint.get_sender_address()->get_type(), AF_INET);
}

TEST(EndpointBase, compares_socket_identity) {
    auto address = std::make_shared<IP4Address>();
    address->set_reliable(true);
    auto socket = std::make_shared<Socket>(address);
    EndpointBase first(socket);
    EndpointBase second(socket);
    EndpointBase empty;

    EXPECT_TRUE(first == second);
    EXPECT_FALSE(first == empty);
}
