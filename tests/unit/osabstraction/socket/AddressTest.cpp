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

#include <socket/Address.h>
#include <arpa/inet.h>
#include <cstring>
#include <gtest/gtest.h>
#include <iostream>
#include <memory>

using namespace lgsomeip::osabstraction;

TEST(Address, get_reliable) {
    std::shared_ptr<Address> addr = std::make_shared<Address>();

    addr->set_reliable(true);
    EXPECT_TRUE(addr->get_reliable());

    addr->set_reliable(false);
    EXPECT_EQ(false, addr->get_reliable());
}

TEST(Address, get_port_address) {
    std::shared_ptr<Address> addr = std::make_shared<Address>();

    addr->set_port_address(8081);
    EXPECT_EQ(8081, addr->get_port_address());

    addr->set_port_address(8082);
    EXPECT_EQ(8082, addr->get_port_address());
}
