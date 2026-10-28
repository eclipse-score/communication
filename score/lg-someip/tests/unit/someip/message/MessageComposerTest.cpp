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
#include <iomanip>
#include <iostream>
#include <memory>

#include <message/Message.h>
#include <socket/IP4Address.h>

using namespace lgsomeip;
using namespace lgsomeip::osabstraction;

TEST(MessageComposer, ComposeApplicationRegisterMessage) {
    std::shared_ptr<MessageSD> message = MessageBuilder::create<SOMEIPSD>();

    SDOption option(SOMEIP_SD_OPTION::CONFIGURATION::TYPEID);
    option.set_configuration(std::string("NAME"), std::string("TestApp"));

    SDEntry entry(SOMEIP_SD_ENTRY::INTERNAL::TYPEID);
    entry.set_option1st_count(1);
    entry.set_ttl(SOMEIP_SD_ENTRY::INTERNAL::REGISTER);

    MessageComposer::add_entry(message, &entry, &option);

    std::uint32_t length;
    std::uint8_t buffer[5000];
    MessageBuilder::build_byte_stream(buffer, &length, *message);
    EXPECT_EQ(length, 62);

    std::uint8_t ret[38] = {0xf0, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x12, 0x00, 0x0f, 0x01, 0x00, 0x0c, 0x4e,
                            0x41, 0x4d, 0x45, 0x3d, 0x54, 0x65, 0x73, 0x74, 0x41, 0x70, 0x70, 0x00};
    EXPECT_EQ(memcmp(ret, buffer + 24, 38), 0);
}

TEST(MessageComposer, ComposeApplicationDeregisterMessage) {
    std::shared_ptr<MessageSD> message = MessageBuilder::create<SOMEIPSD>();

    SDOption option(SOMEIP_SD_OPTION::CONFIGURATION::TYPEID);
    option.set_configuration(std::string("NAME"), std::string("ApplicationTestClient"));

    SDEntry entry(SOMEIP_SD_ENTRY::INTERNAL::TYPEID);
    entry.set_option1st_count(1);
    entry.set_ttl(SOMEIP_SD_ENTRY::INTERNAL::DEREGISTER);

    MessageComposer::add_entry(message, &entry, &option);

    std::uint32_t length;
    std::uint8_t buffer[5000];
    MessageBuilder::build_byte_stream(buffer, &length, *message);
    EXPECT_EQ(length, 76);

    std::uint8_t ret[52] = {0xf0, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x1d, 0x01, 0x00, 0x1a, 0x4e,
                            0x41, 0x4d, 0x45, 0x3d, 0x41, 0x70, 0x70, 0x6c, 0x69, 0x63, 0x61, 0x74, 0x69,
                            0x6f, 0x6e, 0x54, 0x65, 0x73, 0x74, 0x43, 0x6c, 0x69, 0x65, 0x6e, 0x74, 0x00};
    EXPECT_EQ(memcmp(ret, buffer + 24, 52), 0);
}

TEST(MessageComposer, AddsEntryWithoutOptions) {
    auto message = MessageBuilder::create<SOMEIPSD>();
    SDEntry entry(SOMEIP_SD_ENTRY::FINDSERVICE::TYPEID);

    MessageComposer::add_entry(message, &entry);

    ASSERT_EQ(message->entries().size(), 1U);
    EXPECT_EQ(message->options().size(), 0U);
    EXPECT_EQ(message->entry(0).get_option1st_count(), 0);
    EXPECT_EQ(message->entry(0).get_option2nd_count(), 0);
}

TEST(MessageComposer, AddsAndReusesTwoOptions) {
    auto message = MessageBuilder::create<SOMEIPSD>();
    auto first_address = std::make_shared<IP4Address>();
    first_address->set_ip_address("192.0.2.1");
    first_address->set_port_address(30490);
    auto second_address = std::make_shared<IP4Address>();
    second_address->set_ip_address("192.0.2.2");
    second_address->set_port_address(30491);
    SDOption first;
    first.set_address_option(first_address);
    SDOption second;
    second.set_address_option(second_address);
    SDEntry first_entry(SOMEIP_SD_ENTRY::OFFERSERVICE::TYPEID);
    first_entry.set_option1st_count(2);

    MessageComposer::add_entry(message, &first_entry, &first, &second);

    ASSERT_EQ(message->options().size(), 2U);
    EXPECT_EQ(message->entry(0).get_option1st_index(), 0);
    EXPECT_EQ(message->entry(0).get_option1st_count(), 2);

    SDEntry second_entry(SOMEIP_SD_ENTRY::OFFERSERVICE::TYPEID);
    second_entry.set_option1st_count(2);
    MessageComposer::add_entry(message, &second_entry, &first, &second);

    EXPECT_EQ(message->options().size(), 2U);
    EXPECT_EQ(message->entry(1).get_option1st_index(), 0);
    EXPECT_EQ(message->entry(1).get_option1st_count(), 2);
}
