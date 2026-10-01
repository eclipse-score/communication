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

#include <cstring>
#include <gtest/gtest.h>
#include <iostream>
#include <memory>

#include <message/Message.h>

using namespace lgsomeip;
using namespace lgsomeip::osabstraction;

extern std::uint8_t test_message[1000];

TEST(SDOption, get_address_option) {
    auto msg = MessageBuilder::create<SOMEIPSD>();
    MessageBuilder::build_message(*msg, test_message, sizeof(test_message));

    auto addr = msg->option(0).get_address_option();
    EXPECT_EQ(addr->get_ip_address(), std::string("160.48.199.16"));
}

TEST(SDOption, set_address_option) {
    SDOption option;

    std::shared_ptr<Address> addr = std::make_shared<IP4Address>();
    addr->set_ip_address("255.255.255.0");
    addr->set_port_address(30000);
    addr->set_reliable(false);

    option.set_address_option(addr);

    std::uint8_t buf[100];
    std::uint32_t len = option.serialize(buf);

    std::uint8_t ret[12] = {0x00, 0x09, 0x04, 0x00, 0xff, 0xff, 0xff, 0x00, 0x00, 0x11, 0x75, 0x30};

    EXPECT_EQ(len, 12);
    EXPECT_EQ(memcmp(ret, buf, len), 0);
}

TEST(SDOption, Configuration) {
    std::uint8_t payload[19] = {0x00, 0x10, 0x01, 0x00, 0x05, 'a', 'b', 'c', '=', 'x',
                                0x07, 'd',  'e',  'f',  '=',  '1', '2', '3', 0x00};
    SDOption option;
    option.deserialize(payload, 19);

    std::uint8_t buf[100];
    std::uint32_t len = option.serialize(buf);

    EXPECT_EQ(option.get_configuration_count(), 2);
    EXPECT_EQ(option.get_configuration("abc"), std::string("x"));

    EXPECT_EQ(memcmp(payload, buf, len), 0);
}

TEST(SDOption, rejects_truncated_fixed_option) {
    std::uint8_t payload[] = {0x00, 0x09, 0x04};
    SDOption option;

    EXPECT_EQ(option.deserialize(payload, sizeof(payload)), 0U);
}

TEST(SDOption, setConfigurationAndSerialize) {
    std::uint8_t payload[18] = {0x00, 0x0f, 0x01, 0x00, 0x0c, 0x4e, 0x41, 0x4d, 0x45,
                                0x3d, 0x54, 0x65, 0x73, 0x74, 0x41, 0x70, 0x70, 0x00};
    std::uint8_t data[18];

    SDOption option(SOMEIP_SD_OPTION::CONFIGURATION::TYPEID);
    option.set_configuration("NAME", "TestApp");
    int len = option.serialize(data);

    EXPECT_EQ(len, 18); // 15 + 3
    EXPECT_EQ(memcmp(payload, data, 18), 0);
}

TEST(SDOptionTest, setConfigurationAndSerialize2) {
    std::uint8_t payload[32] = {0x00, 0x1d, 0x01, 0x00, 0x1a, 0x4e, 0x41, 0x4d, 0x45, 0x3d, 0x41,
                                0x70, 0x70, 0x6c, 0x69, 0x63, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x54,
                                0x65, 0x73, 0x74, 0x43, 0x6c, 0x69, 0x65, 0x6e, 0x74, 0x00};
    std::uint8_t data[32];

    SDOption option(SOMEIP_SD_OPTION::CONFIGURATION::TYPEID);
    option.set_configuration("NAME", "ApplicationTestClient");
    int len = option.serialize(data);

    EXPECT_EQ(len, 32);
    EXPECT_EQ(memcmp(payload, data, 32), 0);
}

TEST(SDOption, load_balancing_round_trip) {
    SDOption source(SOMEIP_SD_OPTION::LOADBALANCING::TYPEID);
    source.set_priority(0x1234);
    source.set_weight(0x5678);

    std::uint8_t buffer[32] = {};
    ASSERT_EQ(source.serialize(buffer), SOMEIP_SD_OPTION::LOADBALANCING::SIZE);

    SDOption decoded;
    ASSERT_EQ(decoded.deserialize(buffer, sizeof(buffer)), SOMEIP_SD_OPTION::LOADBALANCING::SIZE);
    EXPECT_EQ(decoded.get_type(), SOMEIP_SD_OPTION::LOADBALANCING::TYPEID);
    EXPECT_EQ(decoded.get_priority(), 0x1234);
    EXPECT_EQ(decoded.get_weight(), 0x5678);
}

TEST(SDOption, configuration_can_remove_entries) {
    SDOption option(SOMEIP_SD_OPTION::CONFIGURATION::TYPEID);
    option.set_configuration("A", "1");
    option.set_configuration("B", "2");
    ASSERT_EQ(option.get_configuration_count(), 2U);

    option.remove_configuration("A");

    EXPECT_EQ(option.get_configuration_count(), 1U);
    EXPECT_EQ(option.get_configuration("A"), "");
    EXPECT_EQ(option.get_configuration("B"), "2");
}

TEST(SDOption, ipv6_address_round_trip) {
    auto address = std::make_shared<IP6Address>();
    address->set_ip_address("2001:db8::1");
    address->set_port_address(30490);
    address->set_reliable(true);

    SDOption source;
    source.set_address_option(address);
    std::uint8_t buffer[64] = {};
    const auto serialized_length = source.serialize(buffer);

    SDOption decoded;
    ASSERT_EQ(decoded.deserialize(buffer, serialized_length), serialized_length);
    auto decoded_address = decoded.get_address_option();
    ASSERT_NE(decoded_address, nullptr);
    EXPECT_EQ(decoded_address->get_ip_address(), "2001:db8::1");
    EXPECT_EQ(decoded_address->get_port_address(), 30490);
    EXPECT_TRUE(decoded_address->get_reliable());
}
