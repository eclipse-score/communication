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

class SDEntryTest : public ::testing::Test {
protected:
    virtual void SetUp() {
        message_ = MessageBuilder::create<SOMEIPSD>();
        MessageBuilder::build_message(*message_, test_message, sizeof(test_message));
    }

    std::shared_ptr<MessageSD> message_;
};

TEST_F(SDEntryTest, get_service_id) {
    EXPECT_EQ(message_->entries().back().get_service_id(), 0xb00f);
}

TEST_F(SDEntryTest, get_instance_id) {
    EXPECT_EQ(message_->entries().back().get_instance_id(), 1);
}

TEST_F(SDEntryTest, get_major_version) {
    EXPECT_EQ(message_->entries().back().get_major_version(), 1);
}

TEST(SDEntry, offer_service_round_trip) {
    SDEntry source(SOMEIP_SD_ENTRY::OFFERSERVICE::TYPEID);
    source.set_option1st_index(2);
    source.set_option1st_count(1);
    source.set_option2nd_index(3);
    source.set_option2nd_count(2);
    source.set_service_id(0x1001);
    source.set_instance_id(0x0002);
    source.set_major_version(0x02);
    source.set_minor_version(0x01020304);
    source.set_ttl(0x000405);

    std::uint8_t buffer[SOMEIP_SD_ENTRY::SIZE] = {};
    ASSERT_EQ(source.serialize(buffer), SOMEIP_SD_ENTRY::SIZE);

    SDEntry decoded;
    ASSERT_TRUE(decoded.deserialize(buffer, sizeof(buffer)));
    EXPECT_EQ(decoded.get_type(), SOMEIP_SD_ENTRY::OFFERSERVICE::TYPEID);
    EXPECT_EQ(decoded.get_option1st_index(), 2);
    EXPECT_EQ(decoded.get_option1st_count(), 1);
    EXPECT_EQ(decoded.get_option2nd_index(), 3);
    EXPECT_EQ(decoded.get_option2nd_count(), 2);
    EXPECT_EQ(decoded.get_service_id(), 0x1001);
    EXPECT_EQ(decoded.get_instance_id(), 0x0002);
    EXPECT_EQ(decoded.get_major_version(), 0x02);
    EXPECT_EQ(decoded.get_minor_version(), 0x01020304U);
    EXPECT_EQ(decoded.get_ttl(), 0x000405U);
}

TEST(SDEntry, subscribe_round_trip) {
    SDEntry source(SOMEIP_SD_ENTRY::SUBSCRIBE::TYPEID);
    source.set_service_id(0x1001);
    source.set_instance_id(0x0002);
    source.set_major_version(0x02);
    source.set_ttl(0x000003);
    source.set_flag(0x01);
    source.set_event_group_id(0x4455);

    std::uint8_t buffer[SOMEIP_SD_ENTRY::SIZE] = {};
    source.serialize(buffer);

    SDEntry decoded;
    ASSERT_TRUE(decoded.deserialize(buffer, sizeof(buffer)));
    EXPECT_EQ(decoded.get_type(), SOMEIP_SD_ENTRY::SUBSCRIBE::TYPEID);
    EXPECT_EQ(decoded.get_flag(), 0x01);
    EXPECT_EQ(decoded.get_event_group_id(), 0x4455);
}

TEST(SDEntry, rejects_short_and_unknown_data) {
    SDEntry entry;
    std::uint8_t short_data[SOMEIP_SD_ENTRY::SIZE - 1] = {};
    std::uint8_t unknown_data[SOMEIP_SD_ENTRY::SIZE] = {};
    unknown_data[0] = 0xff;

    EXPECT_FALSE(entry.deserialize(short_data, sizeof(short_data)));
    EXPECT_FALSE(entry.deserialize(unknown_data, sizeof(unknown_data)));
}
