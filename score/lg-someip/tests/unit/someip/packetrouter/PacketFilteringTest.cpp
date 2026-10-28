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

#include <packetrouter/PacketFiltering.h>

using namespace lgsomeip;

TEST(PacketFiltering, passes_first_message_and_releases_latest_blocked_message) {
    PacketFiltering filtering;
    filtering.set_filter_list({{0x1001, 0}});

    std::vector<std::uint8_t> released_message;
    filtering.set_period_expire_func(
        [&released_message](std::shared_ptr<Endpoint>, std::uint8_t* message, std::size_t length) {
            released_message.assign(message, message + length);
        });

    std::uint8_t first_message[] = {0x01, 0x02};
    std::uint8_t blocked_message[] = {0x03, 0x04, 0x05};

    EXPECT_EQ(filtering.filter_message(0x9999, nullptr, first_message, sizeof(first_message)), PacketFiltering::kPass);
    EXPECT_EQ(filtering.filter_message(0x1001, nullptr, first_message, sizeof(first_message)), PacketFiltering::kPass);
    EXPECT_EQ(filtering.filter_message(0x1001, nullptr, blocked_message, sizeof(blocked_message)),
              PacketFiltering::kBlocked);

    TimerManager::get().iterate_loop();

    EXPECT_EQ(released_message, (std::vector<std::uint8_t>{0x03, 0x04, 0x05}));
}
