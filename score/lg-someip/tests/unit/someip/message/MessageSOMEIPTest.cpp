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

TEST(MessageSOMEIP, createSOMEIP) {
    auto msg = MessageBuilder::create<SOMEIP>();
    EXPECT_FALSE(msg->is_service_discovery());
    EXPECT_EQ(msg->get_length(), SOMEIP_HEADER::SIZE);
}
TEST(MessageSOMEIP, payloadTest1) {
    auto msg = MessageBuilder::create<SOMEIP>();

    auto payload = msg->get_payload_type();
    payload->append(static_cast<std::uint16_t>(100));
    payload->append(static_cast<std::uint32_t>(100));
    payload->append(static_cast<std::uint64_t>(100));

    EXPECT_EQ(payload->get_length(), 14);
    const std::uint8_t data[] = {0x00, 0x64, 0x00, 0x00, 0x00, 0x64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x64};
    EXPECT_EQ(std::memcmp(payload->get_payload(), data, 14), 0);

    msg->set_payload(payload);
    EXPECT_EQ(msg->get_length(), SOMEIP_HEADER::SIZE + 14);
}

TEST(MessageSOMEIP, payloadTest2) {
    auto msg = MessageBuilder::create<SOMEIP>();

    const std::uint8_t data[] = {0x00, 0x64, 0x00, 0x00, 0x00, 0x64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x64};
    Payload p1;
    p1.append(data, 14);
    EXPECT_EQ(p1.get_length(), 14);

    msg->get_payload_type()->set_payload(p1);
    EXPECT_EQ(msg->get_length(), SOMEIP_HEADER::SIZE + 14);

    Payload p2;
    p2.append(static_cast<std::uint16_t>(100));
    p2.append(static_cast<std::uint32_t>(100));
    p2.append(static_cast<std::uint64_t>(100));
    EXPECT_EQ(p2 == *msg->get_payload_type(), true);
}
