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

TEST(MessageBuilder, build_message) {
    auto msg = MessageBuilder::create<SOMEIPSD>();
    MessageBuilder::build_message(*msg, test_message, sizeof(test_message));

    // addr->set_reliable(true);
    EXPECT_TRUE(msg->is_service_discovery());
    EXPECT_EQ(msg->entries().size(), 41);
    EXPECT_EQ(msg->options().size(), 1);
}

TEST(MessageBuilder, build_byte_stream) {
    auto msg = MessageBuilder::create<SOMEIPSD>();
    MessageBuilder::build_message(*msg, test_message, sizeof(test_message));

    std::uint32_t length;
    std::uint8_t buffer[1000];
    MessageBuilder::build_byte_stream(buffer, &length, *msg);

    EXPECT_EQ(memcmp(buffer, test_message, length), 0);
}

TEST(MessageBuilder, is_sd_message) {
    auto ret = MessageBuilder::is_sd_message(nullptr, 0);
    EXPECT_EQ(ret, false);
}

TEST(MessageBuilder, is_sd_message_rejects_null_data) {
    EXPECT_FALSE(MessageBuilder::is_sd_message(nullptr, 5));
}

TEST(MessageBuilder, is_sd_message_rejects_short_header) {
    std::uint8_t data[SOMEIP_HEADER::SIZE - 1] = {0xff, 0xff, 0x81, 0x00};

    EXPECT_FALSE(MessageBuilder::is_sd_message(data, sizeof(data)));
}

TEST(MessageBuilder, build_message_rejects_truncated_header) {
    auto msg = MessageBuilder::create<SOMEIP>();
    std::uint8_t data[SOMEIP_HEADER::SIZE - 1] = {};

    EXPECT_FALSE(MessageBuilder::build_message(*msg, data, sizeof(data)));
}

TEST(MessageBuilder, create_notification_message) {
    auto msg = MessageBuilder::create_notification_message(0x1000, 0x0001);
    EXPECT_EQ(msg->get_message_id(), 0x10000001);
    EXPECT_EQ(msg->get_protocol_version(), 0x01);
    EXPECT_EQ(msg->get_message_type(), SOMEIP_MESSAGE_TYPE::NOTIFICATION);
}

TEST(MessageBuilder, create_request_message_variants) {
    auto request = MessageBuilder::create_request_message(0x1001, 0x0001, 0x02);
    ASSERT_NE(request, nullptr);
    EXPECT_EQ(request->get_message_id(), 0x10010001U);
    EXPECT_EQ(request->get_interface_version(), 0x02);
    EXPECT_EQ(request->get_message_type(), SOMEIP_MESSAGE_TYPE::REQUEST);
    EXPECT_EQ(request->get_protocol_version(), 0x01);
    EXPECT_EQ(request->get_return_code(), SOMEIP_RETURN_CODE::E_OK);

    auto no_response = MessageBuilder::create_request_message(0x1001, 0x0001, 0x02, true);
    ASSERT_NE(no_response, nullptr);
    EXPECT_EQ(no_response->get_message_type(), SOMEIP_MESSAGE_TYPE::REQUEST_NO_RETURN);
}

TEST(MessageBuilder, create_response_message_copies_request_identity) {
    auto request = MessageBuilder::create_request_message(0x1001, 0x0001, 0x02);
    request->set_request_id(0x01230045);

    auto response = MessageBuilder::create_response_message(*request);
    ASSERT_NE(response, nullptr);
    EXPECT_EQ(response->get_message_id(), request->get_message_id());
    EXPECT_EQ(response->get_interface_version(), request->get_interface_version());
    EXPECT_EQ(response->get_protocol_version(), request->get_protocol_version());
    EXPECT_EQ(response->get_request_id(), request->get_request_id());
    EXPECT_EQ(response->get_message_type(), SOMEIP_MESSAGE_TYPE::RESPONSE);
    EXPECT_EQ(response->get_return_code(), SOMEIP_RETURN_CODE::E_OK);
}

TEST(MessageBuilder, create_response_message_rejects_non_request) {
    auto notification = MessageBuilder::create_notification_message(0x1001, 0x0001);

    EXPECT_EQ(MessageBuilder::create_response_message(*notification), nullptr);
}
