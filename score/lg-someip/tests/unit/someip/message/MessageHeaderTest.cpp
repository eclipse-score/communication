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

#include <message/MessageHeader.h>
#include <message/MessageConstant.h>

using namespace lgsomeip;

TEST(MessageHeader, serialize_and_deserialize_round_trip) {
    MessageHeader source;
    source.set_message_id(0x12345678);
    source.set_length(0x00000020);
    source.set_request_id(0xABCDEF01);
    source.set_protocol_version(0x01);
    source.set_interface_version(0x02);
    source.set_message_type(SOMEIP_MESSAGE_TYPE::RESPONSE);
    source.set_return_code(SOMEIP_RETURN_CODE::E_OK);

    std::uint8_t buffer[SOMEIP_HEADER::SIZE] = {};
    ASSERT_EQ(source.serialize(buffer), SOMEIP_HEADER::SIZE);

    MessageHeader decoded;
    ASSERT_TRUE(decoded.deserialize(buffer, sizeof(buffer)));
    EXPECT_EQ(decoded.get_message_id(), source.get_message_id());
    EXPECT_EQ(decoded.get_length(), source.get_length());
    EXPECT_EQ(decoded.get_request_id(), source.get_request_id());
    EXPECT_EQ(decoded.get_protocol_version(), source.get_protocol_version());
    EXPECT_EQ(decoded.get_interface_version(), source.get_interface_version());
    EXPECT_EQ(decoded.get_message_type(), source.get_message_type());
    EXPECT_EQ(decoded.get_return_code(), source.get_return_code());
}

TEST(MessageHeader, rejects_null_or_short_input) {
    MessageHeader header;
    std::uint8_t buffer[SOMEIP_HEADER::SIZE] = {};

    EXPECT_FALSE(header.deserialize(nullptr, sizeof(buffer)));
    EXPECT_FALSE(header.deserialize(buffer, SOMEIP_HEADER::SIZE - 1));
}

TEST(MessageHeader, detects_service_discovery_message_id) {
    MessageHeader header;

    header.set_message_id(0xFFFF8100);
    EXPECT_TRUE(header.is_service_discovery());

    header.set_message_id(0x12345678);
    EXPECT_FALSE(header.is_service_discovery());
}
