/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
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

#include <message/MessageConstant.h>
#include <message/MessageSD.h>

using namespace lgsomeip;

TEST(MessageSD, defaults_to_service_discovery)
{
    MessageSD message;

    EXPECT_TRUE(message.is_service_discovery());
    EXPECT_EQ(message.get_message_id(), 0xFFFF8100U);
    EXPECT_EQ(message.get_interface_version(), 0x01);
    EXPECT_EQ(message.get_message_type(), SOMEIP_MESSAGE_TYPE::NOTIFICATION);
    EXPECT_FALSE(message.get_reboot_flag());
}

TEST(MessageSD, serializes_and_deserializes_entries_and_flags)
{
    MessageSD source;
    source.set_flag(SOMEIP_REBOOT_FLAG);

    SDEntry entry(SOMEIP_SD_ENTRY::OFFERSERVICE::TYPEID);
    entry.set_service_id(0x1001);
    entry.set_instance_id(0x0001);
    entry.set_major_version(0x01);
    entry.set_minor_version(0x00000002);
    entry.set_ttl(0x000003);
    source.entries().push_back(entry);

    std::vector<std::uint8_t> buffer(source.get_length() + 16U);
    const std::uint32_t serialized_length = source.serialize(buffer.data());
    ASSERT_GT(serialized_length, SOMEIP_HEADER::SIZE);

    MessageSD decoded;
    ASSERT_TRUE(decoded.deserialize(buffer.data(), serialized_length));
    ASSERT_EQ(decoded.entries().size(), 1U);
    EXPECT_EQ(decoded.get_flag(), SOMEIP_REBOOT_FLAG);
    EXPECT_TRUE(decoded.get_reboot_flag());
    EXPECT_EQ(decoded.entry(0).get_type(), SOMEIP_SD_ENTRY::OFFERSERVICE::TYPEID);
    EXPECT_EQ(decoded.entry(0).get_service_id(), 0x1001);
    EXPECT_EQ(decoded.entry(0).get_instance_id(), 0x0001);
    EXPECT_EQ(decoded.entry(0).get_major_version(), 0x01);
    EXPECT_EQ(decoded.entry(0).get_minor_version(), 0x00000002U);
    EXPECT_EQ(decoded.entry(0).get_ttl(), 0x000003U);
}

TEST(MessageSD, serialize_writes_only_serialized_size)
{
    MessageSD source;
    SDEntry entry(SOMEIP_SD_ENTRY::OFFERSERVICE::TYPEID);
    entry.set_service_id(0x1001);
    source.entries().push_back(entry);

    const std::uint32_t size = source.get_serialized_size();
    std::vector<std::uint8_t> buffer(size + 8U, 0xA5);

    EXPECT_EQ(source.serialize(buffer.data()), size);
    for (std::size_t i = size; i < buffer.size(); ++i)
    {
        EXPECT_EQ(buffer[i], 0xA5) << "guard byte " << i << " was overwritten";
    }
}

TEST(MessageSD, rejects_short_input)
{
    MessageSD message;
    std::uint8_t buffer[SOMEIP_HEADER::SIZE] = {};

    EXPECT_FALSE(message.deserialize(buffer, sizeof(buffer)));
}

TEST(MessageSD, rejects_truncated_sections)
{
    MessageSD message;
    std::uint8_t buffer[SOMEIP_HEADER::SIZE + SOMEIP_SD_HEADER::SIZE] = {};

    buffer[0] = 0xff;
    buffer[1] = 0xff;
    buffer[2] = 0x81;
    buffer[3] = 0x00;
    buffer[7] = 0x04;

    EXPECT_FALSE(message.deserialize(buffer, sizeof(buffer)));
}

TEST(MessageSD, rejects_option_section_outside_message)
{
    MessageSD message;
    std::uint8_t buffer[SOMEIP_HEADER::SIZE + SOMEIP_SD_HEADER::SIZE + 8] = {};

    buffer[0] = 0xff;
    buffer[1] = 0xff;
    buffer[2] = 0x81;
    buffer[3] = 0x00;
    buffer[7] = 0x0c;
    buffer[27] = 0x04;

    EXPECT_FALSE(message.deserialize(buffer, sizeof(buffer)));
}

TEST(MessageSD, keeps_option_indexes_when_skipping_unknown_and_sd_endpoint_options)
{
    const std::uint8_t options[] = {0x00, 0x09, 0x24, 0x00, 0xc0, 0xa8, 0x01, 0x0a, 0x00, 0x11, 0x77, 0x1a,
                                    0x00, 0x03, 0x7e, 0x00, 0xaa, 0xbb,
                                    0x00, 0x09, 0x04, 0x00, 0x0a, 0x00, 0x00, 0x01, 0x00, 0x06, 0x75, 0x30};
    std::vector<std::uint8_t> buffer(SOMEIP_HEADER::SIZE + SOMEIP_SD_HEADER::SIZE + 8 + sizeof(options), 0);
    buffer[0] = 0xff;
    buffer[1] = 0xff;
    buffer[2] = 0x81;
    buffer[3] = 0x00;
    buffer[7] = static_cast<std::uint8_t>(8 + SOMEIP_SD_HEADER::SIZE + 8 + sizeof(options));
    buffer[buffer.size() - sizeof(options) - 1] = static_cast<std::uint8_t>(sizeof(options));
    std::copy(std::begin(options), std::end(options), buffer.end() - sizeof(options));

    MessageSD message;
    ASSERT_TRUE(message.deserialize(buffer.data(), static_cast<std::uint32_t>(buffer.size())));
    ASSERT_EQ(message.options().size(), 3U);
    EXPECT_EQ(message.option(0).get_type(), SOMEIP_SD_OPTION::IP4SD::TYPEID);
    EXPECT_EQ(message.option(1).get_type(), 0x7e);
    EXPECT_EQ(message.option(2).get_type(), SOMEIP_SD_OPTION::IP4::TYPEID);
    EXPECT_EQ(message.option(2).get_address_option()->get_port_address(), 30000);
}
