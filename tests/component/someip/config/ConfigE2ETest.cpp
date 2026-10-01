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
#include <rapidjson/document.h>

#include <config/ConfigE2E.h>
#include <exception/Exception.h>

using namespace lgsomeip;

TEST(ConfigE2E, parses_numeric_values) {
    rapidjson::Document document;
    document.Parse(R"({
        "data_id": 28,
        "variant": "checker",
        "profile": "CRC8",
        "crc_offset": 0,
        "counter_offset": 8,
        "data_id_mode": 0,
        "data_id_nibble_offset": 4,
        "data_length": 56
    })");

    ConfigE2E config(document, 0x7532, 0x8002);

    EXPECT_EQ(config.get_data_id(), 28);
    EXPECT_EQ(config.get_service_id(), 0x7532);
    EXPECT_EQ(config.get_event_id(), 0x8002);
    EXPECT_EQ(config.get_variant(), "checker");
    EXPECT_EQ(config.get_profile(), "CRC8");
    EXPECT_EQ(config.get_crc_offset(), 0);
    EXPECT_EQ(config.get_counter_offset(), 8);
    EXPECT_EQ(config.get_data_id_mode(), 0);
    EXPECT_EQ(config.get_data_id_nibble_offset(), 4);
    EXPECT_EQ(config.get_data_length(), 56);
}

TEST(ConfigE2E, rejects_missing_required_member) {
    rapidjson::Document document;
    document.Parse(R"({
        "data_id": "28",
        "variant": "checker",
        "profile": "CRC8",
        "crc_offset": "0",
        "counter_offset": "8",
        "data_id_mode": "0",
        "data_id_nibble_offset": "0"
    })");

    EXPECT_THROW(ConfigE2E(document, 0x7532, 0x8002), ConfigurationErrorException);
}
