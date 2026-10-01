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

#include <message/MessagePayload.h>

using namespace lgsomeip;

TEST(MessagePayload, appends_big_endian_scalars) {
    MessagePayload payload;

    payload.append(static_cast<std::uint16_t>(0x1122));
    payload.append(static_cast<std::uint32_t>(0x33445566));

    const std::uint8_t expected[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    ASSERT_EQ(payload.get_length(), sizeof(expected));
    EXPECT_EQ(std::memcmp(payload.get_payload(), expected, sizeof(expected)), 0);
}

TEST(MessagePayload, appends_native_order_when_requested) {
    MessagePayload payload;
    const std::uint16_t value = 0x1122;

    payload.append(value, false);

    ASSERT_EQ(payload.get_length(), sizeof(value));
    EXPECT_EQ(std::memcmp(payload.get_payload(), &value, sizeof(value)), 0);
}

TEST(MessagePayload, copies_and_replaces_payload) {
    const std::vector<std::uint8_t> source{1, 2, 3};
    MessagePayload payload;
    payload.append(source);

    MessagePayload copy;
    copy.set_payload(payload);
    EXPECT_TRUE(copy == payload);

    const std::uint8_t replacement[] = {4, 5};
    copy.set_payload(replacement, sizeof(replacement));
    EXPECT_EQ(copy.get_length(), sizeof(replacement));
    EXPECT_EQ(copy.get_payload_vector(), (std::vector<std::uint8_t>{4, 5}));
}

TEST(MessagePayload, empty_payload_is_supported) {
    MessagePayload payload;
    const std::vector<std::uint8_t> empty;

    payload.set_payload(empty);

    EXPECT_EQ(payload.get_length(), 0U);
    EXPECT_EQ(payload.get_payload(), nullptr);
    EXPECT_TRUE(payload == MessagePayload());
}

TEST(MessagePayload, empty_pointer_payload_is_supported) {
    MessagePayload payload;

    payload.set_payload(nullptr, 0);

    EXPECT_EQ(payload.get_length(), 0U);
    EXPECT_EQ(payload.get_payload(), nullptr);
}
