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

#include <socket/NetworkDevice.h>

using namespace lgsomeip::osabstraction;

TEST(NetworkDevice, initialize_rejects_unassigned_address) {
    auto& device = NetworkDevice::instance();

    EXPECT_FALSE(device.initialize("203.0.113.1", 4));
    EXPECT_FALSE(device.is_initialized());
    EXPECT_EQ(device.get_device_name(), "");
    EXPECT_EQ(device.get_device_addr(), "");
    EXPECT_EQ(device.get_device_id(), 0);
}

TEST(NetworkDevice, initialize_loopback) {
    auto& device = NetworkDevice::instance();

    ASSERT_TRUE(device.initialize("127.0.0.1", 4));
    EXPECT_TRUE(device.is_initialized());
    EXPECT_FALSE(device.get_device_name().empty());
    EXPECT_EQ(device.get_device_addr(), "127.0.0.1");
    EXPECT_GT(device.get_device_id(), 0);
}
